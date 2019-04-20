#pragma once
#include <kt/kt.h>

#include <kt/Vec2.h>
#include <kt/Vec3.h>
#include <kt/Vec4.h>

namespace core
{
struct CVarBase;

void InitCVars();
void DrawImGuiCVarMenuItems();

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

template<>
struct CVar<bool> : CVarBase
{
	CVar(char const* _path, char const* _desc, bool _def)
		: CVarBase(_path, _desc)
		, m_default(_def)
		, m_current(_def)
	{
	}

	void DrawImGuiInteraction() override;

	bool HasChanged() const override
	{
		return m_current != m_default;
	}

	void SetDefault() override
	{
		m_current = m_default;
	}

	operator bool() const
	{
		return m_current;
	}


	bool* operator&()
	{
		return &m_current;
	}

	bool const* operator&() const
	{
		return &m_current;
	}

private:
	bool const m_default;
	bool m_current;
};

template<>
struct CVar<float> : CVarBase
{
	CVar(char const* _path, char const* _desc, float _def, float _min, float _max)
		: CVarBase(_path, _desc)
		, m_current(_def)
		, m_default(_def)
		, m_min(_min)
		, m_max(_max)
	{
	}

	void DrawImGuiInteraction() override;

	bool HasChanged() const override
	{
		return m_current != m_default;
	}

	void SetDefault() override
	{
		m_current = m_default;
	}

	operator float() const
	{
		return m_current;
	}


	float* operator&()
	{
		return &m_current;
	}

	float const* operator&() const
	{
		return &m_current;
	}

private:
	float m_current;
	float const m_default;

	float const m_min;
	float const m_max;
};

template<>
struct CVar<kt::Vec2> : CVarBase
{
	CVar(char const* _path, char const* _desc, kt::Vec2 const& _def, float _min, float _max)
		: CVarBase(_path, _desc)
		, m_current(_def)
		, m_default(_def)
		, m_min(_min)
		, m_max(_max)
	{
	}

	void DrawImGuiInteraction() override;

	bool HasChanged() const override
	{
		return m_current != m_default;
	}

	void SetDefault() override
	{
		m_current = m_default;
	}

	operator kt::Vec2 const& ()
	{
		return m_current;
	}

	kt::Vec2* operator&()
	{
		return &m_current;
	}

	kt::Vec2 const* operator&() const
	{
		return &m_current;
	}
	
private:
	kt::Vec2 m_current;
	
	kt::Vec2 const m_default;
	float const m_min;
	float const m_max;
};

template<>
struct CVar<kt::Vec3> : CVarBase
{
	CVar(char const* _path, char const* _desc, kt::Vec3 const& _def, float _min, float _max)
		: CVarBase(_path, _desc)
		, m_current(_def)
		, m_default(_def)
		, m_min(_min)
		, m_max(_max)
	{
	}

	void DrawImGuiInteraction() override;

	bool HasChanged() const override
	{
		return m_current != m_default;
	}

	void SetDefault() override
	{
		m_current = m_default;
	}

	operator kt::Vec3 const& ()
	{
		return m_current;
	}


	kt::Vec3* operator&()
	{
		return &m_current;
	}

	kt::Vec3 const* operator&() const
	{
		return &m_current;
	}

private:
	kt::Vec3 m_current;

	kt::Vec3 const m_default;
	float const m_min;
	float const m_max;
};

template<>
struct CVar<kt::Vec4> : CVarBase
{
	CVar(char const* _path, char const* _desc, kt::Vec4 const& _def, float _min, float _max)
		: CVarBase(_path, _desc)
		, m_current(_def)
		, m_default(_def)
		, m_min(_min)
		, m_max(_max)
	{
	}

	bool HasChanged() const override
	{
		return m_current != m_default;
	}

	void DrawImGuiInteraction() override;

	void SetDefault() override
	{
		m_current = m_default;
	}

	operator kt::Vec4 const& () const
	{
		return m_current;
	}

	kt::Vec4* operator&()
	{
		return &m_current;
	}

	kt::Vec4 const* operator&() const
	{
		return &m_current;
	}

private:
	kt::Vec4 m_current;

	kt::Vec4 const m_default;
	float const m_min;
	float const m_max;
};

struct CVarEnumBase : CVarBase
{
	CVarEnumBase(char const* _name, char const* _desc)
		: CVarBase(_name, _desc)
	{}

	void DrawEnumImGui(char const** _values, uint32_t _numValues, uint32_t& _currentIdx);
};

template <typename EnumT, EnumT EnumMaxT>
struct CVarEnum : CVarEnumBase
{
	CVarEnum(char const* _name, char const* _desc, char const* const (&_enumValues)[size_t(EnumMaxT)], EnumT _default)
		: CVarEnumBase(_name, _desc)
		, m_current(_default)
		, m_default(_default)
		, m_strings(_enumValues)
	{
	}

	void DrawImGuiInteraction() override
	{
		uint32_t curIdx = uint32_t(m_current);
		CVarEnumBase::DrawEnumImGui((char const**)m_strings, uint32_t(EnumMaxT), curIdx);
		m_current = EnumT(curIdx);
	}

	bool HasChanged() const override
	{
		return m_current != m_default;
	}

	void SetDefault() override
	{
		m_current = m_default;
	}

	operator EnumT () const
	{
		return m_current;
	}

	EnumT* operator&()
	{
		return &m_current;
	}

	EnumT const* operator&() const
	{
		return &m_current;
	}

private:
	EnumT m_current;

	EnumT const m_default;
	char const* const (&m_strings)[size_t(EnumMaxT)];
};

}