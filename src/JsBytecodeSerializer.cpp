#include "JsBytecodeSerializer.hpp"
#include "Value.hpp"
#include "OpCodeInfo.hpp"
#include <sstream>
#include <fstream>
#include <cstdio>
#include <algorithm>

// ============================================================================
// JSON bytecode serializer (PRC1) -- see JsBytecodeSerializer.hpp for the
// format description.
//
// This used to live in PyRiteJS/cpp/ as a file that was never compiled: it
// was written against a parallel, camelCase API (CompiledProgram, ValueType,
// fn.numParams) that the PyRite compiler never had. It is rewritten here
// against the real structures: BytecodeChunk / CompiledFunction / the
// polymorphic Value hierarchy.
// ============================================================================

namespace {

// ---- JSON string escaping ----
std::string escapeJsonString(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    for (size_t i = 0; i < s.size(); i++) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    return out;
}

// ---- Serialize one Value into a JSON constant object ----
// Type tags match PyRiteJS/src/runtime/PyValue.js (TYPE).
std::string serializeValue(const ValuePtr& v) {
    if (!v) return "{\"t\":0}";

    if (dynamic_cast<NullValue*>(v.get())) {
        return "{\"t\":0}";
    }
    if (dynamic_cast<NumberValue*>(v.get())) {
        // Kept as a string so BigNumber precision survives the JSON round trip.
        NumberValue* n = static_cast<NumberValue*>(v.get());
        return "{\"t\":1,\"v\":\"" + escapeJsonString(n->value.toString()) + "\"}";
    }
    if (dynamic_cast<StringValue*>(v.get())) {
        StringValue* s = static_cast<StringValue*>(v.get());
        return "{\"t\":2,\"v\":\"" + escapeJsonString(s->value) + "\"}";
    }
    if (dynamic_cast<LnValue*>(v.get())) {
        LnValue* l = static_cast<LnValue*>(v.get());
        std::string out = "{\"t\":4,\"v\":[";
        for (size_t i = 0; i < l->elements.size(); i++) {
            if (i > 0) out += ",";
            out += serializeValue(l->elements[i]);
        }
        out += "]}";
        return out;
    }
    if (dynamic_cast<DimValue*>(v.get())) {
        DimValue* d = static_cast<DimValue*>(v.get());
        std::string out = "{\"t\":5,\"v\":{";
        bool first = true;
        for (std::map<std::string, ValuePtr>::const_iterator it = d->dict.begin();
             it != d->dict.end(); ++it) {
            if (!first) out += ",";
            first = false;
            out += "\"" + escapeJsonString(it->first) + "\":" + serializeValue(it->second);
        }
        out += "}}";
        return out;
    }
    if (dynamic_cast<BinaryValue*>(v.get())) {
        BinaryValue* b = static_cast<BinaryValue*>(v.get());
        std::string out = "{\"t\":10,\"v\":[";
        for (size_t i = 0; i < b->value.size(); i++) {
            if (i > 0) out += ",";
            out += std::to_string(static_cast<int>(b->value[i]));
        }
        out += "]}";
        return out;
    }

    // Functions, native functions, bound methods, exceptions and classes have
    // no representation in the constant format. They are never emitted as
    // constants by the compiler (functions are referenced through OP_CLOSURE),
    // so this is a defensive fallback rather than a normal path.
    return "{\"t\":0}";
}

// ---- Serialize one CompiledFunction ----
std::string serializeFunction(const CompiledFunctionPtr& fn, bool is_exposed,
                              bool include_lines) {
    std::ostringstream oss;
    oss << "{";
    oss << "\"name\":\"" << escapeJsonString(fn->name) << "\",";

    // code: raw bytecode bytes
    oss << "\"code\":[";
    for (size_t i = 0; i < fn->code.size(); i++) {
        if (i > 0) oss << ",";
        oss << static_cast<int>(fn->code[i]);
    }
    oss << "],";

    // constants: the literal pool
    oss << "\"constants\":[";
    for (size_t i = 0; i < fn->constants.size(); i++) {
        if (i > 0) oss << ",";
        oss << serializeValue(fn->constants[i]);
    }
    oss << "],";

    // lines: source line per bytecode byte (optional)
    if (include_lines) {
        oss << "\"lines\":[";
        for (size_t i = 0; i < fn->lines.size(); i++) {
            if (i > 0) oss << ",";
            oss << fn->lines[i];
        }
        oss << "],";
    }

    oss << "\"arity\":" << fn->arity << ",";
    oss << "\"num_params\":" << fn->num_params << ",";
    oss << "\"upvalue_count\":" << fn->upvalue_count << ",";
    oss << "\"max_locals\":" << fn->max_locals << ",";

    oss << "\"upvalue_descriptors\":[";
    for (size_t i = 0; i < fn->upvalue_descriptors.size(); i++) {
        if (i > 0) oss << ",";
        const UpvalueDesc& d = fn->upvalue_descriptors[i];
        oss << "{\"is_local\":" << (d.is_local ? "true" : "false")
            << ",\"index\":" << d.index << "}";
    }
    oss << "],";

    oss << "\"params\":[";
    for (size_t i = 0; i < fn->params.size(); i++) {
        if (i > 0) oss << ",";
        const CompiledFunction::ParamInfo& p = fn->params[i];
        oss << "{\"name\":\"" << escapeJsonString(p.name)
            << "\",\"has_default\":" << (p.has_default ? "true" : "false");
        if (p.has_default && p.default_value) {
            oss << ",\"default\":" << serializeValue(p.default_value);
        } else {
            oss << ",\"default\":null";
        }
        oss << "}";
    }
    oss << "],";

    oss << "\"local_names\":[";
    for (size_t i = 0; i < fn->local_names.size(); i++) {
        if (i > 0) oss << ",";
        oss << "\"" << escapeJsonString(fn->local_names[i]) << "\"";
    }
    oss << "],";

    oss << "\"is_method\":" << (fn->is_method ? "true" : "false") << ",";
    oss << "\"is_exposed\":" << (is_exposed ? "true" : "false") << ",";
    // Which constants hold spoken lines. The packaging step watermarks
    // exactly these.
    oss << "\"display_constants\":[";
    for (size_t i = 0; i < fn->display_constants.size(); i++) {
        if (i > 0) oss << ",";
        oss << fn->display_constants[i];
    }
    oss << "]";
    oss << "}";
    return oss.str();
}

bool containsName(const std::vector<std::string>& names, const std::string& n) {
    return std::find(names.begin(), names.end(), n) != names.end();
}

// Walk a function's bytecode and report the first opcode the JavaScript VM
// cannot execute. Returns true and fills 'why' when one is found.
//
// The operand table comes from opcodes.def (OpCodeInfo.hpp), so this walker
// cannot fall out of step with the instruction encoding.
bool find_js_unsupported(const CompiledFunctionPtr& fn, std::string& why) {
    const std::vector<uint8_t>& code = fn->code;
    size_t ip = 0;
    while (ip < code.size()) {
        uint8_t raw = code[ip];
        if (raw >= OPCODE_COUNT) {
            why = std::string("an invalid opcode (") + std::to_string((int)raw) + ")";
            return true;
        }
        OpCode op = static_cast<OpCode>(raw);
        if (OPCODE_JS_UNSUPPORTED[raw]) {
            why = std::string(op_name(op)) + " (" + opcode_js_unsupported_reason(op) + ")";
            return true;
        }
        ip++;
        if (OPCODE_HAS_OPERAND[raw]) {
            if (ip + 3 > code.size()) {
                why = "a truncated operand";
                return true;
            }
            ip += 3;
        }
    }
    return false;
}

} // namespace

std::string serializeToJson(BytecodeChunkPtr chunk, const JsonExportInfo& info) {
    if (!chunk) return "{}";

    std::ostringstream oss;
    oss << "{";
    oss << "\"magic\":\"PRC1\",";
    oss << "\"version\":1,";

    oss << "\"all_functions\":[";
    for (size_t i = 0; i < chunk->all_functions.size(); i++) {
        if (i > 0) oss << ",";
        const CompiledFunctionPtr& fn = chunk->all_functions[i];
        bool exposed = containsName(info.exposed_functions, fn->name);
        oss << serializeFunction(fn, exposed, info.include_lines);
    }
    oss << "],";

    // The entry function is the script itself; it is always index 0 in a
    // freshly compiled chunk, but look it up rather than assume it.
    int entry = 0;
    for (size_t i = 0; i < chunk->all_functions.size(); i++) {
        if (chunk->all_functions[i] == chunk->script) { entry = static_cast<int>(i); break; }
    }
    oss << "\"entry_function\":" << entry << ",";

    oss << "\"exports\":[";
    for (size_t i = 0; i < info.exports.size(); i++) {
        if (i > 0) oss << ",";
        oss << "\"" << escapeJsonString(info.exports[i]) << "\"";
    }
    oss << "]";
    oss << "}";
    return oss.str();
}

JsonExportResult writeJsonToFile(BytecodeChunkPtr chunk, const std::string& path,
                                 const JsonExportInfo& info) {
    JsonExportResult result;
    result.ok = false;
    result.error.clear();

    if (!chunk) {
        result.error = "nothing to export";
        return result;
    }

    // Refuse to emit bytecode the JavaScript VM cannot execute.
    for (size_t i = 0; i < chunk->all_functions.size(); i++) {
        std::string why;
        if (find_js_unsupported(chunk->all_functions[i], why)) {
            std::string where = (chunk->all_functions[i] == chunk->script)
                ? std::string("the script")
                : ("function '" + chunk->all_functions[i]->name + "'");
            result.error = where + " uses " + why +
                           ", which the JavaScript VM cannot execute";
            return result;
        }
    }

    std::string json = serializeToJson(chunk, info);
    std::ofstream out(path.c_str(), std::ios::binary);
    if (!out.is_open()) {
        result.error = "cannot open " + path + " for writing";
        return result;
    }
    out << json;
    out.close();
    if (out.fail()) {
        result.error = "cannot write " + path;
        return result;
    }
    result.ok = true;
    return result;
}
