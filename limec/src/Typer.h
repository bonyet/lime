#pragma once

class Typer
{
public:
	static std::vector<struct Type*> definedTypes;

	template<typename T>
	static T* Add(const std::string& typeName)
	{
		static_assert(std::is_base_of<Type, T>::value, "cannot resolve type");

		definedTypes.push_back(new T(typeName));
		return static_cast<T*>(definedTypes.back());
	}

	static Type* Get(const std::string& typeName);
	static bool Exists(const std::string& typeName);
	static std::vector<Type*>& GetAll() { return definedTypes; }

	static void Release();

};