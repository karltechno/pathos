#include "CVar.h"

#include <kt/Strings.h>
#include <kt/Array.h>
#include <kt/Sort.h>
#include <kt/Slice.h>
#include <kt/Logging.h>

#include "imgui.h"

namespace core
{

CVarBase* CVarBase::s_head;

uint32_t CVarBase::s_numVars;

namespace CVarDrawHelpers
{
void DrawIntImGui(CVarBase* _base, void* _intPtr, void const* _intMin, void const* _intMax, uint32_t _typeSize, bool _isSigned)
{
	ImGuiDataType dataType = 0;

	switch (_typeSize)
	{
		case 1: dataType = _isSigned ? ImGuiDataType_S8 : ImGuiDataType_U8; break;
		case 2: dataType = _isSigned ? ImGuiDataType_S16 : ImGuiDataType_U16; break;
		case 4: dataType = _isSigned ? ImGuiDataType_S32 : ImGuiDataType_U32; break;
		case 8: dataType = _isSigned ? ImGuiDataType_S64 : ImGuiDataType_U64; break;

		default:
		{
			KT_ASSERT(false);
			return;
		} break;
	}

	ImGui::DragScalar(_base->PathSuffix(), dataType, _intPtr, 1.0f, _intMin, _intMax);
}

void DrawEnumImGui(CVarBase* _base, char const** _values, uint32_t _numValues, uint32_t& _currentIdx)
{
	ImGui::PushID(_base);
	int curIdx = int(_currentIdx);
	ImGui::Combo(_base->PathSuffix(), &curIdx, _values, int(_numValues));
	_currentIdx = uint32_t(curIdx);
	ImGui::PopID();
}


}

struct CVarTreeNode
{
	enum class Type
	{
		Group,
		Leaf
	} m_type;

	kt::StringView m_pathPrefix;
	kt::StringView m_pathPart;

	kt::Array<CVarTreeNode> m_group;
	CVarBase* m_leaf;
};

struct CVarContext
{
	CVarTreeNode m_root;
};

static CVarContext s_ctx;

static CVarBase** RegisterCVarGroupsRecursive(CVarTreeNode& _parent, kt::Slice<CVarBase*> _vars)
{
	if (_vars.Empty())
	{
		return _vars.End();
	}
	
	kt::StringView const& parentStr = _parent.m_pathPrefix;

	for (CVarBase** it = _vars.Begin(); 
		 it != _vars.End();
		 /* */)
	{
		CVarBase* var = *it;
		kt::StringView pathView = var->m_path;
		if (pathView.Size() < parentStr.Size()
			|| kt::StrCmpI(parentStr, kt::StringView(var->m_path, parentStr.Size())) != 0)
		{
			return it;
		}

		kt::StringView pathAfterParent = kt::StringView(var->m_path + parentStr.Size());

		if (!parentStr.Empty())
		{
			if (pathAfterParent.Empty() || (pathAfterParent[0] != '.' || pathAfterParent.Size() == 1))
			{
				KT_LOG_ERROR("Invalid CVar path: \"%s\" - expected '.*****' after parent path: \"%.*s\"", var->m_path, parentStr.Size(), parentStr.Data());
				KT_ASSERT(false);
				it = ++it;
				continue;
			}
			pathAfterParent = pathAfterParent.Suffix(1);
		}
		else if (pathAfterParent.Empty())
		{
			KT_LOG_ERROR("Empty CVar node!");
			KT_ASSERT(false);
			it = ++it;
			continue;
		}

		kt::StringView const nextPathSeg = kt::StrFind(pathAfterParent, '.');

		if (nextPathSeg.Empty())
		{
			// Leaf node
			bool anyDupe = false;
			for (CVarTreeNode const& sibling : _parent.m_group)
			{
				if (sibling.m_type == CVarTreeNode::Type::Leaf)
				{
					if (kt::StrCmpI(pathAfterParent, sibling.m_pathPart) == 0)
					{
						KT_LOG_ERROR("Duplicate CVarNode found: %s", var->m_path);
						KT_ASSERT(false);
						anyDupe = true;
						break;
					}
				}
			}

			if (!anyDupe)
			{
				CVarTreeNode& leafNode = _parent.m_group.PushBack();
				leafNode.m_pathPart = pathAfterParent;
				leafNode.m_pathPrefix = var->m_path;
				leafNode.m_type = CVarTreeNode::Type::Leaf;
				leafNode.m_leaf = var;
			}

			it = ++it;
		}
		else
		{
			CVarTreeNode& parentNode = _parent.m_group.PushBack();
			parentNode.m_pathPart = kt::StringView(pathAfterParent.Begin(), nextPathSeg.Begin());
			parentNode.m_pathPrefix = kt::StringView(var->m_path, nextPathSeg.Begin());
			parentNode.m_type = CVarTreeNode::Type::Group;
			it = RegisterCVarGroupsRecursive(parentNode, kt::MakeSlice(it, _vars.End()));
		}
	}

	return _vars.End();
}

void InitCVars()
{
	s_ctx.m_root.m_type = CVarTreeNode::Type::Group;

	if (!CVarBase::s_numVars)
	{
		return;
	}

	CVarBase** varArray = (CVarBase**)KT_ALLOCA(sizeof(CVarBase*) * CVarBase::s_numVars);
	CVarBase* head = CVarBase::s_head;

	for (uint32_t i = 0; i < CVarBase::s_numVars; ++i)
	{
		varArray[i] = head;
		head = head->m_next;
	}

	kt::QuickSort(varArray, varArray + CVarBase::s_numVars, [](CVarBase* _lhs, CVarBase* _rhs)
	{
		return kt::StrCmpI(_lhs->m_path, _rhs->m_path) == -1;
	});

	RegisterCVarGroupsRecursive(s_ctx.m_root, kt::MakeSlice(varArray, CVarBase::s_numVars));
}

void ImGuiDrawTreeRecursive(CVarTreeNode const& _group)
{
	ImGui::PushID(&_group);
	if (_group.m_type == CVarTreeNode::Type::Group)
	{
		kt::String128 str;
		str.AppendFmt("%.*s", _group.m_pathPart.Size(), _group.m_pathPart.Data());

		if (ImGui::BeginMenu(str.Data()))
		{
			for (CVarTreeNode const& node : _group.m_group)
			{
				ImGuiDrawTreeRecursive(node);
			}

			ImGui::EndMenu();
		}
	}
	else
	{
		static const ImVec4 c_defaultCvarCol = ImVec4(0.0f, 0.8f, 0.2f, 1.0f);
		static const ImVec4 c_changedCvarCol = ImVec4(1.0f, 0.7f, 0.2f, 1.0f);
		ImVec4 const col = _group.m_leaf->HasChanged() ? c_changedCvarCol : c_defaultCvarCol;
		ImGui::PushStyleColor(ImGuiCol_Text, col);
		_group.m_leaf->DrawImGuiInteraction();
		ImGui::PopStyleColor();


		if (ImGui::BeginPopupContextItem(_group.m_leaf->PathSuffix()))
		{
			ImGui::Text("Path: %s", _group.m_leaf->m_path);
			ImGui::Text("Description: %s", _group.m_leaf->m_desc);
			if (ImGui::Button("Reset"))
			{
				_group.m_leaf->SetDefault();
			}
			ImGui::EndPopup();
		}
	}
	ImGui::PopID();
}

void DrawImGuiCVarMenuItems()
{
	if (ImGui::BeginMenu("CVars"))
	{
		for (CVarTreeNode const& node : s_ctx.m_root.m_group)
		{
			ImGuiDrawTreeRecursive(node);
		}
		ImGui::EndMenu();
	}
}

void CVar<bool>::DrawImGuiInteraction()
{
	ImGui::Checkbox(PathSuffix(), &m_current);
}

void CVar<float>::DrawImGuiInteraction()
{
	ImGui::SliderFloat(PathSuffix(), &m_current, m_min, m_max);
}

void CVar<kt::Vec2>::DrawImGuiInteraction()
{
	ImGui::SliderFloat2(PathSuffix(), &m_current[0], m_min, m_max);
}

void CVar<kt::Vec3>::DrawImGuiInteraction()
{
	ImGui::SliderFloat3(PathSuffix(), &m_current[0], m_min, m_max);
}

void CVar<kt::Vec4>::DrawImGuiInteraction()
{
	ImGui::SliderFloat4(PathSuffix(), &m_current[0], m_min, m_max);
}



char const* CVarBase::PathSuffix() const
{
	kt::StringView const suffix = kt::StrFindR(m_path, '.');
	return suffix.Empty() ? m_path : suffix.Data() + 1;
}




}