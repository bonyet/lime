#pragma once

class Typer
{
public:
	static std::vector<Type*> definedTypes;

	template<typename T, typename... Args>
	static T* Add(Args&&... args)
	{
		static_assert(std::is_base_of<Type, T>::value, "Cannot resolve type");

		definedTypes.push_back(new T(std::forward<Args>(args)...));
		return static_cast<T*>(definedTypes.back());
	}

	static Type* Get(const std::string& typeName)
	{
		Type* result = nullptr;
		if (!Valid(typeName, &result))
		{
			printf("Type '%s' not registered\n", typeName.c_str());
			return definedTypes.front();
		}

		return result;
	}
	static std::vector<Type*>& GetAll() { return definedTypes; }

	static bool Valid(const std::string& typeName, Type** out)
	{
		for (Type* type : definedTypes)
		{
			if (type->name == typeName)
			{
				*out = type;
				return true;
			}
		}

		out = nullptr;
		return false;
	}

	static void Release()
	{
		for (Type* type : definedTypes)
			delete type;
	}
};