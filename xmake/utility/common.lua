import("core.cache.detectcache")
---@alias map_t table<string, string>

---Determine if a toolchain is clang
---@return boolean --Is the clang toolchain
function is_clang()
    return string.find(get_config("toolchain") or "", "clang", 1, true) ~= nil
end

---Cache keys used in this module
local cache_key = "toolchain.utility"

---Getting Cache Information
---@return table<string, any> --Cache Information Table
function get_cache()
    local cache_info = detectcache:get(cache_key)
    if not cache_info then
        cache_info = {}
        detectcache:set(cache_key, cache_info)
    end
    return cache_info
end

---Updating Cache Information
---@param cache_info table --Cache information to be saved
---@return nil
function update_cache(cache_info)
    detectcache:set(cache_key, cache_info)
    detectcache:save()
end

---Get a normalized boolean config value
---@param name string --Config name
---@param default boolean | nil --Default value when config is unset
---@return boolean --Normalized boolean value
function get_config_bool(name, default)
    local value = get_config(name)
    if value == nil then
        return default == true
    end
    if type(value) == "boolean" then
        return value
    end
    if type(value) == "string" then
        local normalized = string.lower(string.trim(value))
        if normalized == "y" or normalized == "yes" or normalized == "true" or normalized == "on" or normalized == "1" then
            return true
        end
        if normalized == "n" or normalized == "no" or normalized == "false" or normalized == "off" or normalized == "0" then
            return false
        end
    end
    return value and true or false
end
