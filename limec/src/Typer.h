#pragma once

class Typer
{
public:
	static std::vector<Type*> definedTypes;

	template<typename T>
	static T* Add(const std::string& typeName)
	{
		static_assert(std::is_base_of<Type, T>::value, "cannot resolve type");

		definedTypes.push_back(new T(typeName));
		return static_cast<T*>(definedTypes.back());
	}

	static Type* Get(const std::string& typeName)
	{
		Type* result = nullptr;

		// Make sure we have registered the type - or throw
		for (Type* type : definedTypes)
		{
			if (type->name == typeName)
			{
				result = type;
			}
		}

		if (!result)
			throw LimeError("type '%s' not registered\n", typeName.c_str());

		return result;
	}

	static std::vector<Type*>& GetAll() { return definedTypes; }

	static bool Exists(const std::string& typeName)
	{
		for (Type* type : definedTypes)
		{
			if (type->name == typeName)
				return true;
		}

		return false;
	}

	static void Release()
	{
		for (Type* type : definedTypes)
		{
			delete type;
		}

		definedTypes.clear();
	}
};