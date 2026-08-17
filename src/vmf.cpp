#include "vmfpropmerger/vmf.h"
#include "vmfpropmerger/transform.h"

namespace vmfpropmerger {

std::vector<VMFToken> tokenizeVMF(const std::string& text) {
    std::vector<VMFToken> tokens;
    size_t i = 0;
    while (i < text.size()) {
        const char c = text[i];
        if (std::isspace(static_cast<unsigned char>(c))) { ++i; continue; }
        if (c == '/' && i + 1 < text.size() && text[i + 1] == '/') {
            i += 2;
            while (i < text.size() && text[i] != '\n') ++i;
            continue;
        }
        if (c == '{') { tokens.push_back({VMFTokenType::OpenBrace, "{"}); ++i; continue; }
        if (c == '}') { tokens.push_back({VMFTokenType::CloseBrace, "}"}); ++i; continue; }
        if (c == '"') {
            ++i;
            std::string value;
            while (i < text.size()) {
                const char ch = text[i++];
                if (ch == '"') break;
                if (ch == '\\' && i < text.size()) {
                    const char next = text[i];
                    if (next == '"' || next == '\\') { value.push_back(next); ++i; }
                    else value.push_back(ch);
                } else {
                    value.push_back(ch);
                }
            }
            tokens.push_back({VMFTokenType::String, value});
            continue;
        }
        std::string value;
        while (i < text.size() && !std::isspace(static_cast<unsigned char>(text[i])) &&
               text[i] != '{' && text[i] != '}') {
            value.push_back(text[i++]);
        }
        if (!value.empty()) tokens.push_back({VMFTokenType::String, value});
    }
    return tokens;
}

bool parseVMFObjectContents(const std::vector<VMFToken>& tokens, size_t& pos, VMFObject& output) {
    if (pos >= tokens.size() || tokens[pos].type != VMFTokenType::OpenBrace) return false;
    ++pos;
    while (pos < tokens.size()) {
        if (tokens[pos].type == VMFTokenType::CloseBrace) { ++pos; return true; }
        if (tokens[pos].type != VMFTokenType::String) return false;
        const std::string name = tokens[pos++].text;
        if (pos >= tokens.size()) return false;
        if (tokens[pos].type == VMFTokenType::String) {
            const std::string value = tokens[pos++].text;
            output.keys.push_back({name, value});
            continue;
        }
        if (tokens[pos].type == VMFTokenType::OpenBrace) {
            VMFObject child;
            child.name = name;
            if (!parseVMFObjectContents(tokens, pos, child)) return false;
            output.children.push_back(std::move(child));
            continue;
        }
        return false;
    }
    return false;
}

bool parseVMFObject(const std::vector<VMFToken>& tokens, size_t& pos, VMFObject& output) {
    if (pos >= tokens.size() || tokens[pos].type != VMFTokenType::String) return false;
    output.name = tokens[pos++].text;
    return parseVMFObjectContents(tokens, pos, output);
}

bool parseVMF(const fs::path& path, std::vector<VMFObject>& roots) {
    std::string contents;
    if (!readWholeFile(path, contents)) {
        std::cerr << "Could not open VMF file:\n" << path << "\n";
        return false;
    }
    if (contents.empty()) {
        std::cerr << "VMF file is empty:\n" << path << "\n";
        return false;
    }
    const std::vector<VMFToken> tokens = tokenizeVMF(contents);
    if (tokens.empty()) {
        std::cerr << "VMF tokenizer produced no tokens.\n";
        return false;
    }
    size_t pos = 0;
    while (pos < tokens.size()) {
        VMFObject root;
        if (!parseVMFObject(tokens, pos, root)) {
            std::cerr << "VMF parse error near token " << pos << " of " << tokens.size() << ".\n";
            if (pos < tokens.size())
                std::cerr << "Token: \"" << tokens[pos].text << "\"\n";
            return false;
        }
        roots.push_back(std::move(root));
    }
    return !roots.empty();
}

std::string escapeVMF(const std::string& value) {
    std::string result;
    for (char c : value) {
        if (c == '"' || c == '\\') result += '\\';
        result += c;
    }
    return result;
}

void writeVMFObject(std::ostream& output, const VMFObject& obj, int indent) {
    const std::string pad(static_cast<size_t>(indent), '\t');
    output << pad << obj.name << "\n";
    output << pad << "{\n";
    for (const auto& kv : obj.keys) {
        output << pad << "\t\"" << escapeVMF(kv.key) << "\" \"" << escapeVMF(kv.value) << "\"\n";
    }
    for (const auto& child : obj.children) {
        writeVMFObject(output, child, indent + 1);
    }
    output << pad << "}\n";
}

bool getKey(const VMFObject& obj, const std::string& key, std::string& value) {
    const std::string wanted = lowerString(key);
    for (const auto& kv : obj.keys) {
        if (lowerString(kv.key) == wanted) { value = kv.value; return true; }
    }
    return false;
}

void setKey(VMFObject& obj, const std::string& key, const std::string& value) {
    const std::string wanted = lowerString(key);
    for (auto& kv : obj.keys) {
        if (lowerString(kv.key) == wanted) { kv.value = value; return; }
    }
    obj.keys.push_back({key, value});
}

std::vector<PropDynamic> collectProps(const std::vector<VMFObject>& roots) {
    std::vector<PropDynamic> result;
    uint64_t entityId = 0;
    std::function<void(const VMFObject&)> visit;
    visit = [&](const VMFObject& obj) {
        if (lowerString(obj.name) == "entity") {
            std::string classname;
            if (getKey(obj, "classname", classname) && lowerString(classname) == "prop_dynamic") {
                PropDynamic prop;
                prop.object = obj;
                prop.entityId = entityId++;
                getKey(obj, "model", prop.model);
                std::string value;
                if (getKey(obj, "origin", value)) prop.origin = parseVector(value, {});
                if (getKey(obj, "angles", value)) prop.angles = parseVector(value, {});
                if (getKey(obj, "modelscale", value)) prop.modelScale = parseDouble(value, 1.0);
                if (getKey(obj, "modelscale_axis", value)) prop.modelScaleAxis = parseVector(value, {1.0,1.0,1.0});
                if (!prop.model.empty()) result.push_back(std::move(prop));
            }
        }
        for (const auto& child : obj.children) visit(child);
    };
    for (const auto& root : roots) visit(root);
    return result;
}

void collectNamedEntities(const std::vector<VMFObject>& roots,
                          std::map<std::string, const VMFObject*>& entities) {
    std::function<void(const VMFObject&)> visit;
    visit = [&](const VMFObject& object) {
        if (lowerString(object.name) == "entity") {
            std::string targetName;
            if (getKey(object, "targetname", targetName) && !targetName.empty()) {
                const std::string normalized = lowerString(trim(targetName));
                if (entities.find(normalized) == entities.end()) {
                    entities.emplace(normalized, &object);
                }
            }
        }
        for (const VMFObject& child : object.children) visit(child);
    };
    for (const VMFObject& root : roots) visit(root);
}

bool getLocalEntityTransform(const VMFObject& object, Vec3& origin, RotationMatrix& rotation) {
    origin = {};
    Vec3 angles = {};
    std::string value;
    if (getKey(object, "origin", value)) origin = parseVector(value, {});
    if (getKey(object, "angles", value)) angles = parseVector(value, {});
    rotation = sourceAngleMatrix(angles);
    return true;
}

bool resolveEntityWorldTransform(const VMFObject& object,
                                 const std::map<std::string, const VMFObject*>& entities,
                                 Vec3& worldOrigin,
                                 RotationMatrix& worldRotation,
                                 std::set<const VMFObject*>& resolving) {
    if (resolving.find(&object) != resolving.end()) {
        std::cerr << "WARNING: Parent cycle detected while resolving entity.\n";
        return false;
    }
    resolving.insert(&object);
    Vec3 localOrigin;
    RotationMatrix localRotation;
    getLocalEntityTransform(object, localOrigin, localRotation);
    std::string parentName;
    if (!getKey(object, "parentname", parentName) || trim(parentName).empty()) {
        worldOrigin = localOrigin;
        worldRotation = localRotation;
        resolving.erase(&object);
        return true;
    }
    const std::size_t comma = parentName.find(',');
    if (comma != std::string::npos) parentName = parentName.substr(0, comma);
    parentName = trim(parentName);
    const std::string normalizedParent = lowerString(parentName);
    auto parentIt = entities.find(normalizedParent);
    if (parentIt == entities.end()) {
        std::cerr << "WARNING: Could not resolve parent \"" << parentName << "\". Using local transform.\n";
        worldOrigin = localOrigin;
        worldRotation = localRotation;
        resolving.erase(&object);
        return true;
    }
    Vec3 parentOrigin;
    RotationMatrix parentRotation;
    if (!resolveEntityWorldTransform(*parentIt->second, entities, parentOrigin, parentRotation, resolving)) {
        worldOrigin = localOrigin;
        worldRotation = localRotation;
        resolving.erase(&object);
        return true;
    }
    worldRotation = multiplyRotationMatrices(parentRotation, localRotation);
    worldOrigin = parentOrigin + rotateVectorSource(localOrigin, parentRotation);
    resolving.erase(&object);
    return true;
}

bool isPropDynamic(const VMFObject& object) {
    if (lowerString(object.name) != "entity") return false;
    std::string classname;
    return getKey(object, "classname", classname) && lowerString(classname) == "prop_dynamic";
}

std::string getModelName(const VMFObject& object) {
    std::string model;
    getKey(object, "model", model);
    return model;
}

void removeMergedProps(VMFObject& object,
                       const std::unordered_set<uint64_t>& mergedEntityIds,
                       uint64_t& nextEntityId) {
    std::vector<VMFObject> kept;
    kept.reserve(object.children.size());
    for (auto& child : object.children) {
        bool remove = false;
        if (isPropDynamic(child)) {
            const uint64_t currentId = nextEntityId++;
            if (mergedEntityIds.find(currentId) != mergedEntityIds.end()) {
                remove = true;
            }
        } else {
            removeMergedProps(child, mergedEntityIds, nextEntityId);
        }
        if (!remove) kept.push_back(std::move(child));
    }
    object.children = std::move(kept);
}

} // namespace vmfpropmerger