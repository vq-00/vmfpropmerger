#pragma once

#include "vmfpropmerger/utils.h"
#include "vmfpropmerger/transform.h"

namespace vmfpropmerger {

enum class VMFTokenType {
    String,
    OpenBrace,
    CloseBrace
};

struct VMFToken {
    VMFTokenType type;
    std::string text;
};

struct VMFKeyValue {
    std::string key;
    std::string value;
};

struct VMFObject {
    std::string name;
    std::vector<VMFKeyValue> keys;
    std::vector<VMFObject> children;
};

std::vector<VMFToken> tokenizeVMF(const std::string& text);
bool parseVMFObjectContents(const std::vector<VMFToken>& tokens, size_t& pos, VMFObject& output);
bool parseVMFObject(const std::vector<VMFToken>& tokens, size_t& pos, VMFObject& output);
bool parseVMF(const fs::path& path, std::vector<VMFObject>& roots);

std::string escapeVMF(const std::string& value);
void writeVMFObject(std::ostream& output, const VMFObject& obj, int indent);

bool getKey(const VMFObject& obj, const std::string& key, std::string& value);
void setKey(VMFObject& obj, const std::string& key, const std::string& value);

struct PropDynamic {
    VMFObject object;
    std::string model;
    Vec3 origin;
    Vec3 angles;
    Vec3 worldOrigin;
    RotationMatrix worldRotation;
    bool hasParent = false;
    std::string parentName;
    double modelScale = 1.0;
    Vec3 modelScaleAxis = {1.0, 1.0, 1.0};
    uint64_t entityId = 0;
};

std::vector<PropDynamic> collectProps(const std::vector<VMFObject>& roots);

void collectNamedEntities(const std::vector<VMFObject>& roots,
                          std::map<std::string, const VMFObject*>& entities);

bool getLocalEntityTransform(const VMFObject& object, Vec3& origin, RotationMatrix& rotation);

bool resolveEntityWorldTransform(const VMFObject& object,
                                 const std::map<std::string, const VMFObject*>& entities,
                                 Vec3& worldOrigin,
                                 RotationMatrix& worldRotation,
                                 std::set<const VMFObject*>& resolving);

bool isPropDynamic(const VMFObject& object);
std::string getModelName(const VMFObject& object);
void removeMergedProps(VMFObject& object,
                       const std::unordered_set<uint64_t>& mergedEntityIds,
                       uint64_t& nextEntityId);

} // namespace vmfpropmerger