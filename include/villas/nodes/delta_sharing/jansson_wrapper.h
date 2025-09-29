#ifndef __JANSSON_WRAPPER_H__
#define __JANSSON_WRAPPER_H__

#include <jansson.h>
#include <string>
#include <map>
#include <vector>
#include <memory>
#include <stdexcept>

namespace DeltaSharing {
namespace JanssonWrapper {

// RAII wrapper for json_t
class JsonValue {
private:
    json_t* m_json;

public:
    JsonValue() : m_json(nullptr) {}

    explicit JsonValue(json_t* json) : m_json(json) {
        if (m_json) {
            json_incref(m_json);
        }
    }

    //Reference Counting
    JsonValue(const JsonValue& other) : m_json(other.m_json) {
        if (m_json) {
            json_incref(m_json);
        }
    }

    JsonValue& operator=(const JsonValue& other) {
        if (this != &other) {
            if (m_json) {
                json_decref(m_json);
            }
            m_json = other.m_json;
            if (m_json) {
                json_incref(m_json);
            }
        }
        return *this;
    }

    ~JsonValue() {
        if (m_json) {
            json_decref(m_json);
        }
    }

    json_t* get() const { return m_json; }
    json_t* release() {
        json_t* result = m_json;
        m_json = nullptr;
        return result;
    }

    // Type checking
    bool is_object() const { return json_is_object(m_json); }
    bool is_array() const { return json_is_array(m_json); }
    bool is_string() const { return json_is_string(m_json); }
    bool is_integer() const { return json_is_integer(m_json); }
    bool is_real() const { return json_is_real(m_json); }
    bool is_boolean() const { return json_is_boolean(m_json); }
    bool is_null() const { return json_is_null(m_json); }

    std::string string_value() const {
        if (!is_string()) return "";
        return std::string(json_string_value(m_json));
    }

    int integer_value() const {
        if (!is_integer()) return 0;
        return static_cast<int>(json_integer_value(m_json));
    }

    double real_value() const {
        if (!is_real()) return 0.0;
        return json_real_value(m_json);
    }

    bool boolean_value() const {
        return json_is_true(m_json);
    }

    bool contains(const std::string& key) const {
        return json_object_get(m_json, key.c_str()) != nullptr;
    }

    JsonValue operator[](const std::string& key) const {
        json_t* value = json_object_get(m_json, key.c_str());
        return JsonValue(value);
    }

    size_t size() const {
        if (is_array()) {
            return json_array_size(m_json);
        } else if (is_object()) {
            return json_object_size(m_json);
        }
        return 0;
    }

    JsonValue operator[](size_t index) const {
        if (!is_array()) return JsonValue();
        return JsonValue(json_array_get(m_json, index));
    }

    // Iteration through json items
    class iterator {
    private:
        json_t* m_json;
        void* m_iter;
        bool m_is_array;
        size_t m_array_index;
        size_t m_array_size;

    public:
        iterator(json_t* json, bool is_array = false)
            : m_json(json), m_iter(nullptr), m_is_array(is_array), m_array_index(0), m_array_size(0) {
            if (m_json) {
                if (m_is_array) {
                    m_array_size = json_array_size(m_json);
                    if (m_array_size > 0) {
                        m_iter = json_array_get(m_json, 0);
                    }
                } else {
                    m_iter = json_object_iter(m_json);
                }
            }
        }

        iterator& operator++() {
            if (m_is_array) {
                m_array_index++;
                if (m_array_index < m_array_size) {
                    m_iter = json_array_get(m_json, m_array_index);
                } else {
                    m_iter = nullptr;
                }
            } else {
                m_iter = json_object_iter_next(m_json, m_iter);
            }
            return *this;
        }

        bool operator!=(const iterator& other) const {
            return m_iter != other.m_iter;
        }

        JsonValue operator*() const {
            return JsonValue(static_cast<json_t*>(m_iter));
        }

        std::string key() const {
            if (m_is_array) return std::to_string(m_array_index);
            return std::string(json_object_iter_key(m_iter));
        }
    };

    iterator begin() const {
        return iterator(m_json, is_array());
    }

    iterator end() const {
        return iterator(nullptr, is_array());
    }

    // Conversion to string
    std::string dump() const {
        char* str = json_dumps(m_json, JSON_COMPACT);
        if (!str) return "";
        std::string result(str);
        free(str);
        return result;
    }

    // Static factory methods
    static JsonValue parse(const std::string& json_str) {
        json_error_t error;
        json_t* json = json_loads(json_str.c_str(), 0, &error);
        if (!json) {
            throw std::runtime_error("JSON parse error: " + std::string(error.text));
        }
        return JsonValue(json);
    }

    static JsonValue parse_file(const std::string& filename) {
        json_error_t error;
        json_t* json = json_load_file(filename.c_str(), 0, &error);
        if (!json) {
            throw std::runtime_error("JSON file error: " + std::string(error.text));
        }
        return JsonValue(json);
    }

    static JsonValue object() {
        return JsonValue(json_object());
    }

    static JsonValue array() {
        return JsonValue(json_array());
    }

    static JsonValue string(const std::string& value) {
        return JsonValue(json_string(value.c_str()));
    }

    static JsonValue integer(int value) {
        return JsonValue(json_integer(value));
    }

    static JsonValue real(double value) {
        return JsonValue(json_real(value));
    }

    static JsonValue boolean(bool value) {
        return JsonValue(value ? json_true() : json_false());
    }

    static JsonValue null() {
        return JsonValue(json_null());
    }

    // Object manipulation
    // void set(const std::string& key, const JsonValue& value) {
    void set(const std::string& key, JsonValue value) {
        if (is_object()) {
            json_object_set_new(m_json, key.c_str(), value.release());
        }
    }

    // Array manipulation
    // void push_back(const JsonValue& value) {
    void push_back(JsonValue value) {
        if (is_array()) {
            json_array_append_new(m_json, value.release());
        }
    }

    bool empty() const {
        return m_json == nullptr || size() == 0;
    }
};

using json = JsonValue;

} // namespace JanssonWrapper
} // namespace DeltaSharing

#endif
