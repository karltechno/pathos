#pragma once
#include <kt/kt.h>

#include <kt/Vec2.h>
#include <kt/Vec3.h>
#include <kt/Vec4.h>

#include <type_traits>

namespace core
{
struct CVarBase;

void InitCVars();
void DrawImGuiCVarMenuItems();

namespace CVarDrawHelpers
{
void DrawIntImGui(CVarBase* _base, void* _intPtr, void const* _intMin, void const* _intMax, uint32_t _typeSize, bool _isSigned);
void DrawEnumImGui(CVarBase* _base, char const** _values, uint32_t _numValues, uint32_t& _currentIdx);
}

struct CVarBase
{
	CVarBase(char const* _path, char const* _desc)
		: m_path(_path)
		, m_desc(_desc)
	{
		m_next = s_head;
		s_head = this;
		++s_numVars;
	}

	char const* PathSuffix() const;

	virtual void DrawImGuiInteraction() = 0;
	virtual void SetDefault() = 0;
	virtual bool HasChanged() const = 0;

	char const* m_path;
	char const* m_desc;
	CVarBase* m_next = nullptr;

	static CVarBase* s_head;
	static uint32_t s_numVars;
};

template <typename T>
struct CVar : CVarBase
{
	CVar()
	{
		static_assert(false, "TweakVar should be specialized.");
	}
};

template <typename T>
struct CVarTypedBase : CVarBase
{
	CVarTypedBase(char const* _path, char const* _desc, T _default)
		: CVarBase(_path, _desc)
		, m_current(_default)
		, m_default(_default)
	{
	}

	bool HasChanged() const override
	{
		return m_current != m_default;
	}

	void SetDefault() override
	{
		m_current = m_default;
	}

	operator T() const
	{
		return m_current;
	}

	T* operator&()
	{
		return &m_current;
	}

	T const* operator&() const
	{
		return &m_current;
	}

	T GetValue() const
	{
		return m_current;
	}

protected:
	T m_current;

	T const m_default;
};

template<>
struct CVar<bool> : CVarTypedBase<bool>
{
	CVar(char const* _path, char const* _desc, bool _def)
		: CVarTypedBase<bool>(_path, _desc, _def)
	{
	}

	void DrawImGuiInteraction() override;
};

template<>
struct CVar<float> : CVarTypedBase<float>
{
	CVar(char const* _path, char const* _desc, float _def, float _min, float _max)
		: CVarTypedBase<float>(_path, _desc, _def)
		, m_min(_min)
		, m_max(_max)
	{
	}

	void DrawImGuiInteraction() override;


private:
	float const m_min;
	float const m_max;
};

template<>
struct CVar<kt::Vec2> : CVarTypedBase<kt::Vec2>
{
	CVar(char const* _path, char const* _desc, kt::Vec2 const& _def, float _min, float _max)
		: CVarTypedBase<kt::Vec2>(_path, _desc, _def)
		, m_min(_min)
		, m_max(_max)
	{
	}

	void DrawImGuiInteraction() override;
	
private:
	float const m_min;
	float const m_max;
};

template<>
struct CVar<kt::Vec3> : CVarTypedBase<kt::Vec3>
{
	CVar(char const* _path, char const* _desc, kt::Vec3 const& _def, float _min, float _max)
		: CVarTypedBase<kt::Vec3>(_path, _desc, _def)
		, m_min(_min)
		, m_max(_max)
	{
	}

	void DrawImGuiInteraction() override;

private:
	float const m_min;
	float const m_max;
};
template<>
struct CVar<kt::Vec4> : CVarTypedBase<kt::Vec4>
{
	CVar(char const* _path, char const* _desc, kt::Vec4 const& _def, float _min, float _max)
		: CVarTypedBase<kt::Vec4>(_path, _desc, _def)
		, m_min(_min)
		, m_max(_max)
	{
	}

	void DrawImGuiInteraction() override;

private:
	float const m_min;
	float const m_max;
};


template <typename EnumT, EnumT EnumMaxT>
struct CVarEnum : CVarTypedBase<EnumT>
{
	CVarEnum(char const* _name, char const* _desc, char const* const (&_enumValues)[size_t(EnumMaxT)], EnumT _default)
		: CVarTypedBase<EnumT>(_name, _desc, _default)
		, m_strings(_enumValues)
	{
	}

	void DrawImGuiInteraction() override
	{
		uint32_t curIdx = uint32_t(CVarTypedBase<EnumT>::m_current);
		CVarDrawHelpers::DrawEnumImGui(this, (char const**)m_strings, uint32_t(EnumMaxT), curIdx);
		CVarTypedBase<EnumT>::m_current = EnumT(curIdx);
	}

private:
	char const* const (&m_strings)[size_t(EnumMaxT)];
};


template <typename IntT>
struct CVarIntTemplated : CVarTypedBase<IntT>
{
	CVarIntTemplated(char const* _path, char const* _desc, IntT _default, IntT _min, IntT _max)
		: CVarTypedBase<IntT>(_path, _desc, _default)
		, m_min(_min)
		, m_max(_max)
	{}

	void DrawImGuiInteraction() override
	{
		CVarDrawHelpers::DrawIntImGui(this, (void*)&(CVarTypedBase<IntT>::m_current), (void const*)&m_min, (void const*)&m_max, sizeof(IntT), std::is_signed<IntT>::value);
	}


private:
	IntT const m_min;
	IntT const m_max;
};


template <>
struct CVar<uint8_t> : CVarIntTemplated<uint8_t>
{
	CVar(char const* _path, char const* _desc, uint8_t _default, uint8_t _min, uint8_t _max)
		: CVarIntTemplated<uint8_t>(_path, _desc, _default, _min, _max)
	{
	}
};

template <>
struct CVar<int8_t> : CVarIntTemplated<int8_t>
{
	CVar(char const* _path, char const* _desc, int8_t _default, int8_t _min, int8_t _max)
		: CVarIntTemplated<int8_t>(_path, _desc, _default, _min, _max)
	{
	}
};

template <>
struct CVar<uint16_t> : CVarIntTemplated<uint16_t>
{
	CVar(char const* _path, char const* _desc, uint16_t _default, uint16_t _min, uint16_t _max)
		: CVarIntTemplated<uint16_t>(_path, _desc, _default, _min, _max)
	{
	}
};

template <>
struct CVar<int16_t> : CVarIntTemplated<int16_t>
{
	CVar(char const* _path, char const* _desc, int16_t _default, int16_t _min, int16_t _max)
		: CVarIntTemplated<int16_t>(_path, _desc, _default, _min, _max)
	{
	}
};

template <>
struct CVar<uint32_t> : CVarIntTemplated<uint32_t>
{
	CVar(char const* _path, char const* _desc, uint32_t _default, uint32_t _min, uint32_t _max)
		: CVarIntTemplated<uint32_t>(_path, _desc, _default, _min, _max)
	{
	}
};

template <>
struct CVar<int32_t> : CVarIntTemplated<int32_t>
{
	CVar(char const* _path, char const* _desc, int32_t _default, int32_t _min, int32_t _max)
		: CVarIntTemplated<int32_t>(_path, _desc, _default, _min, _max)
	{
	}
};

template <>
struct CVar<uint64_t> : CVarIntTemplated<uint64_t>
{
	CVar(char const* _path, char const* _desc, uint64_t _default, uint64_t _min, uint64_t _max)
		: CVarIntTemplated<uint64_t>(_path, _desc, _default, _min, _max)
	{
	}
};

template <>
struct CVar<int64_t> : CVarIntTemplated<int64_t>
{
	CVar(char const* _path, char const* _desc, int64_t _default, int64_t _min, int64_t _max)
		: CVarIntTemplated<int64_t>(_path, _desc, _default, _min, _max)
	{
	}
};


}