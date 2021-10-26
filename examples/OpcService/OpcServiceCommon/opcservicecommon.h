#ifndef OPCSERVICECOMMON_H
#define OPCSERVICECOMMON_H

#include "opcservicecommon_global.h"
#include "variantpropertytree.h"
#include <Wt/Json/Object>
#include <Wt/Json/Value>
#include <Wt/Json/Parser>
#include <Wt/Json/Serializer>

namespace MRL {

// the root directory
constexpr const char * RootDir = "/usr/local/MRL5/OpcService";

/**
 * The OpcServiceCommon class
 * singletons shared by objects
 * mostly configuration
 */
class OpcServiceCommon {
    std::string _name; // server name
    VariantPropertyTree _data; // shared data in a property tree
    static OpcServiceCommon *_instance; // configuration singleton

public:
    OpcServiceCommon();

    /**
     * instance
     * @return 
     */
    static OpcServiceCommon *instance() {
        if (!_instance) _instance = new OpcServiceCommon();
        return _instance;
    }

    /**
     * data
     * @return 
     */
    static VariantPropertyTree &data() {
        return _instance->_data;
    }

    /**
     * name
     * @return 
     */
    const std::string &name() {
        return  _name;
    }

    /**
     * Save the named configuration
     * @return true on success
     */
    static bool saveConfiguration(const std::string &n);

    /**
     * Save site settings
     * @return true on success
     */
    static bool saveSettings();

    /**
     * load the named configuration
     * @return true on success
     */
    static bool loadConfiguration(const std::string &n = "");

    /**
     * load site settings
     * @return true on success
     */
    static bool loadSettings();

    /**
     * settingFileName
     * @return 
     */
    static std::string settingFileName(const std::string &n)
    {
        std::string f =  std::string(MRL::RootDir) + "/data/" + n + ".setting";
        return f;
    }

    /**
     * globalFileName
     * @return 
     */
    static std::string globalFileName()
    {
        std::string f =  std::string(MRL::RootDir) + "/data/settings.global";
        return f;
    }
};

//Some helpers

template <typename T>
/**
 * stringToNumber
 * @param Text
 * @return 
 */
inline T stringToNumber(const std::string &Text) { //Text not by const reference so that the function can be used with a
    if (!Text.empty()) {
        //character array as argument
        std::stringstream ss(Text);
        T result;
        return ss >> result ? result : T(0);
    }
    return T(0);
}

/**
 * stringToBool
 * @param s
 * @return 
 */
bool stringToBool(const std::string &s);

/**
 * boolToString
 * @param f
 * @return 
 */
inline const char *boolToString(bool f) {
    return f ? "True" : "False";
}

/**
 * MRL::stringTimeToInt
 * parse a string in hh:mm:ss format to a string - allows for missing sections
 * hence ss mm:ss and hh:mm:ss are OK. No range validation for parts - usually gives what is actually wanted
 * @param s
 * @return time in seconds
 */
int stringTimeToInt(const std::string &s);

/**
 * stringToJson
 * @param s
 * @param v
 * @return 
 */
inline bool stringToJson(const std::string &s, Wt::Json::Object &v) {
    if (!s.empty()) {
        try {
            Wt::Json::parse(s,v);
            return true;
        }
        catch (const std::exception &e) {
            EXCEPT_TRC
        }
        catch (...) {
            EXCEPT_DEF
        }
    }
    return false;
}

/**
 * jsonToString
 * @param v
 * @param s
 * @return 
 */
inline bool jsonToString(Wt::Json::Object &v, std::string &s) {
    try {
        s = Wt::Json::serialize(v);
        return true;
    }
    catch (const std::exception &e) {
        EXCEPT_TRC
    }
    catch (...) {
        EXCEPT_DEF
    }
    return false;
}

} // namespace MRL

#endif // OPCSERVICECOMMON_H
