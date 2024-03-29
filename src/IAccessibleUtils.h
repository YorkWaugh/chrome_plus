#ifndef IACCESSIBLEUTILS_H_
#define IACCESSIBLEUTILS_H_

#include <oleacc.h>
#pragma comment(lib, "oleacc.lib")

#include <thread>
#include <wrl/client.h>

using NodePtr = Microsoft::WRL::ComPtr<IAccessible>;

template <typename Function>
void GetAccessibleName(NodePtr node, Function f)
{
    VARIANT self;
    self.vt = VT_I4;
    self.lVal = CHILDID_SELF;

    BSTR bstr = nullptr;
    if (S_OK == node->get_accName(self, &bstr))
    {
        f(bstr);
        SysFreeString(bstr);
    }
    else
    {
        DebugLog(L"GetAccessibleName failed");
    }
}

template <typename Function>
void GetAccessibleDescription(NodePtr node, Function f)
{
    VARIANT self;
    self.vt = VT_I4;
    self.lVal = CHILDID_SELF;

    BSTR bstr = nullptr;
    if (S_OK == node->get_accDescription(self, &bstr))
    {
        f(bstr);
        SysFreeString(bstr);
        DebugLog(L"GetAccessibleDescription succeeded");
    }
    else
    {
        DebugLog(L"GetAccessibleDescription failed");
    }
}

long GetAccessibleRole(NodePtr node)
{
    VARIANT self;
    self.vt = VT_I4;
    self.lVal = CHILDID_SELF;

    VARIANT role;
    if (S_OK == node->get_accRole(self, &role))
    {
        if (role.vt == VT_I4)
        {
            return role.lVal;
        }
    }
    return 0;
}

long GetAccessibleState(NodePtr node)
{
    VARIANT self;
    self.vt = VT_I4;
    self.lVal = CHILDID_SELF;

    VARIANT state;
    if (S_OK == node->get_accState(self, &state))
    {
        if (state.vt == VT_I4)
        {
            return state.lVal;
        }
    }
    return 0;
}

template <typename Function>
void TraversalAccessible(NodePtr node, Function f, bool rawTraversal = false)
{
    if (!node)
        return;

    long childCount = 0;
    if (S_OK != node->get_accChildCount(&childCount) || !childCount)
        return;

    std::vector<VARIANT> varChildren(childCount);
    if (S_OK != AccessibleChildren(node.Get(), 0, childCount, varChildren.data(), &childCount))
        return;

    for (const auto &varChild : varChildren)
    {
        if (varChild.vt != VT_DISPATCH)
            continue;

        Microsoft::WRL::ComPtr<IDispatch> dispatch = varChild.pdispVal;
        NodePtr child = nullptr;
        if (S_OK != dispatch->QueryInterface(IID_IAccessible, (void **)&child))
            continue;

        if (rawTraversal)
        {
            TraversalAccessible(child, f, true);
            if (f(child))
                break;
        }
        else
        {
            if ((GetAccessibleState(child) & STATE_SYSTEM_INVISIBLE) == 0) // 只遍历可见节点
            {
                if (f(child))
                    break;
            }
        }
    }
}

NodePtr FindElementWithRole(NodePtr node, long role)
{
    NodePtr element = nullptr;
    if (node)
    {
        TraversalAccessible(node, [&](NodePtr child) {
            if (auto childRole = GetAccessibleRole(child); childRole == role)
            {
                element = child;
            }
            else
            {
                element = FindElementWithRole(child, role);
            }
            return element != nullptr;
        });
    }
    return element;
}

NodePtr GetParentElement(NodePtr child)
{
    NodePtr element = nullptr;
    Microsoft::WRL::ComPtr<IDispatch> dispatch = nullptr;
    if (S_OK == child->get_accParent(&dispatch) && dispatch)
    {
        NodePtr parent = nullptr;
        if (S_OK == dispatch->QueryInterface(IID_IAccessible, (void **)&parent))
        {
            element = parent;
        }
    }
    return element;
}

NodePtr GetTopContainerView(HWND hwnd)
{
    NodePtr TopContainerView = nullptr;
    wchar_t name[MAX_PATH];
    if (GetClassName(hwnd, name, MAX_PATH) &&
        wcsstr(name, L"Chrome_WidgetWin_") == name)
    {
        NodePtr paccMainWindow = nullptr;
        if (S_OK == AccessibleObjectFromWindow(hwnd, OBJID_WINDOW,
                                               IID_PPV_ARGS(&paccMainWindow)))
        {
            NodePtr PageTabList = FindElementWithRole(paccMainWindow, ROLE_SYSTEM_PAGETABLIST);
            if (PageTabList)
            {
                TopContainerView = GetParentElement(PageTabList);
            }
            if (!TopContainerView)
            {
                DebugLog(L"GetTopContainerView failed");
            }
        }
    }
    return TopContainerView;
}

NodePtr GetMenuBarPane(HWND hwnd)
{
    NodePtr MenuBarPane = nullptr;
    wchar_t name[MAX_PATH];
    if (GetClassName(hwnd, name, MAX_PATH) &&
        wcsstr(name, L"Chrome_WidgetWin_") == name)
    {
        NodePtr paccMainWindow = nullptr;
        if (S_OK == AccessibleObjectFromWindow(hwnd, OBJID_WINDOW,
                                               IID_PPV_ARGS(&paccMainWindow)))
        {
            NodePtr MenuBar = FindElementWithRole(paccMainWindow, ROLE_SYSTEM_MENUBAR);
            if (MenuBar)
            {
                MenuBarPane = GetParentElement(MenuBar);
            }
            if (!MenuBarPane)
            {
                DebugLog(L"GetBookmarkView failed");
            }
        }
    }
    return MenuBarPane;
}

NodePtr FindChildElement(NodePtr parent, long role, int skipcount = 0)
{
    NodePtr element = nullptr;
    if (parent)
    {
        int i = 0;
        TraversalAccessible(parent,
                            [&element, &role, &i, &skipcount](NodePtr child) {
                                // DebugLog(L"当前 %d,%d", i, skipcount);
                                if (GetAccessibleRole(child) == role)
                                {
                                    if (i == skipcount)
                                    {
                                        element = child;
                                    }
                                    i++;
                                }
                                return element != nullptr;
                            });
    }
    return element;
}

template <typename Function>
void GetAccessibleSize(NodePtr node, Function f)
{
    VARIANT self;
    self.vt = VT_I4;
    self.lVal = CHILDID_SELF;

    RECT rect;
    if (S_OK == node->accLocation(&rect.left, &rect.top, &rect.right,
                                  &rect.bottom, self))
    {
        auto [left, top, right, bottom] = rect;
        f({left, top, right + left, bottom + top});
    }
}

// 鼠标是否在某个标签上
bool IsOnOneTab(NodePtr top, POINT pt)
{
    bool flag = false;
    NodePtr PageTabList = FindElementWithRole(top, ROLE_SYSTEM_PAGETABLIST);
    if (PageTabList)
    {
        NodePtr PageTab = FindElementWithRole(PageTabList, ROLE_SYSTEM_PAGETAB);
        if (PageTab)
        {
            NodePtr PageTabPane = GetParentElement(PageTab);
            if (PageTabPane)
            {
                TraversalAccessible(PageTabPane, [&flag, &pt](NodePtr child) {
                    if (GetAccessibleRole(child) == ROLE_SYSTEM_PAGETAB)
                    {
                        GetAccessibleSize(child, [&flag, &pt](RECT rect) {
                            if (PtInRect(&rect, pt))
                            {
                                flag = true;
                            }
                        });
                    }
                    return flag;
                });
            }
        }
    }
    return flag;
}

// 是否只有一个标签
bool IsOnlyOneTab(NodePtr top)
{
    if (!IsKeepLastTabFun())
    {
        return false;
    }

    NodePtr PageTabList = FindElementWithRole(top, ROLE_SYSTEM_PAGETABLIST);
    if (!PageTabList)
    {
        return false;
    }

    NodePtr PageTab = FindElementWithRole(PageTabList, ROLE_SYSTEM_PAGETAB);
    if (!PageTab)
    {
        return false;
    }

    NodePtr PageTabPane = GetParentElement(PageTab);
    if (!PageTabPane)
    {
        return false;
    }

    std::vector<NodePtr> children;
    TraversalAccessible(PageTabPane, [&children](NodePtr child) {
        children.push_back(child);
        return false;
    });

    auto tab_count = std::count_if(children.begin(), children.end(), [](NodePtr child) {
        auto role = GetAccessibleRole(child);
        auto state = GetAccessibleState(child);
        return role == ROLE_SYSTEM_PAGETAB ||
               (role == ROLE_SYSTEM_PAGETABLIST && (state & STATE_SYSTEM_COLLAPSED));
    });

    return tab_count <= 1;
}

// 鼠标是否在标签栏上
bool IsOnTheTabBar(NodePtr top, POINT pt)
{
    bool flag = false;
    NodePtr PageTabList = FindElementWithRole(top, ROLE_SYSTEM_PAGETABLIST);
    if (PageTabList)
    {
        GetAccessibleSize(PageTabList, [&flag, &pt](RECT rect) {
            if (PtInRect(&rect, pt))
            {
                flag = true;
            }
        });
    }
    return flag;
}

// 当前激活标签是否是新标签页
bool IsOnNewTab(NodePtr top)
{
    if (!IsNewTabDisableFun())
    {
        return false;
    }

    bool flag = false;
    std::unique_ptr<wchar_t, decltype(&free)> new_tab_name(nullptr, free);
    NodePtr PageTabList = FindElementWithRole(top, ROLE_SYSTEM_PAGETABLIST);
    if (!PageTabList)
    {
        return false;
    }

    TraversalAccessible(PageTabList, [&new_tab_name](NodePtr child) {
        if (GetAccessibleRole(child) == ROLE_SYSTEM_PUSHBUTTON)
        {
            GetAccessibleName(child, [&new_tab_name](BSTR bstr) {
                new_tab_name.reset(_wcsdup(bstr));
            });
        }
        return false;
    });

    NodePtr PageTab = FindElementWithRole(PageTabList, ROLE_SYSTEM_PAGETAB);
    if (!PageTab)
    {
        return false;
    }

    NodePtr PageTabPane = GetParentElement(PageTab);
    if (!PageTabPane)
    {
        return false;
    }

    TraversalAccessible(
        PageTabPane, [&flag, &new_tab_name](NodePtr child) {
            if (GetAccessibleState(child) & STATE_SYSTEM_SELECTED)
            {
                GetAccessibleName(child, [&flag, &new_tab_name](BSTR bstr) {
                    std::wstring_view bstr_view(bstr);
                    std::wstring_view new_tab_view(new_tab_name.get());
                    flag = (bstr_view.compare(0, new_tab_view.size(), new_tab_view) == 0) ||
                           (bstr_view.substr(0, 11) == L"about:blank");
                });
            }
            return false;
        });
    return flag;
}

// 鼠标是否在书签上
bool IsOnBookmark(NodePtr top, POINT pt)
{
    bool flag = false;
    if (top)
    {
        TraversalAccessible(top, [&flag, &pt](NodePtr child) {
            if (GetAccessibleRole(child) == ROLE_SYSTEM_PUSHBUTTON)
            {
                GetAccessibleSize(child, [&flag, &pt, &child](RECT rect) {
                    if (PtInRect(&rect, pt))
                    {
                        GetAccessibleDescription(child, [&flag](BSTR bstr) {
                            std::wstring_view bstr_view(bstr);
                            flag = bstr_view.find_first_of(L".:") != std::wstring_view::npos &&
                                    bstr_view.substr(0, 11) != L"javascript:";
                        });
                    }
                });
            }
            return flag;
        }, true); // rawTraversal
    }
    return flag;
}

// 鼠标是否在菜单栏的书签文件（夹）上
bool IsOnMenuBookmark(NodePtr top, POINT pt)
{
    bool flag = false;
    NodePtr MenuBar = FindElementWithRole(top, ROLE_SYSTEM_MENUBAR);
    if (MenuBar)
    {
        NodePtr MenuItem = FindElementWithRole(MenuBar, ROLE_SYSTEM_MENUITEM);
        if (MenuItem)
        {
            NodePtr MenuBarPane = GetParentElement(MenuItem);
            if (MenuBarPane)
            {
                TraversalAccessible(
                    MenuBarPane, [&flag, &pt, &MenuBarPane](NodePtr child) {
                        if (GetAccessibleRole(child) == ROLE_SYSTEM_MENUITEM)
                        {
                            GetAccessibleSize(child, [&flag, &pt, &child](RECT rect) {
                                if (PtInRect(&rect, pt))
                                {
                                    GetAccessibleDescription(child, [&flag](BSTR bstr) {
                                        if (bstr &&
                                            ((wcsstr(bstr, L".") != nullptr || wcsstr(bstr, L":") != nullptr) &&
                                             wcsstr(bstr, L"javascript:") != bstr))
                                        {
                                            flag = true;
                                        }
                                    });
                                }
                            });
                        }
                        return flag;
                    });
            }
        }
    }
    return flag;
}

// 地址栏是否已经获得焦点
bool IsOmniboxFocus(NodePtr top)
{
    bool flag = false;
    NodePtr ToolBar = FindElementWithRole(top, ROLE_SYSTEM_TOOLBAR);
    if (ToolBar)
    {
        NodePtr OmniboxEdit = FindElementWithRole(ToolBar, ROLE_SYSTEM_TEXT);
        if (OmniboxEdit)
        {
            NodePtr ToolBarGroup = GetParentElement(OmniboxEdit);
            if (ToolBarGroup)
            {
                TraversalAccessible(ToolBarGroup, [&flag](NodePtr child) {
                    if (GetAccessibleRole(child) == ROLE_SYSTEM_TEXT)
                    {
                        if (GetAccessibleState(child) & STATE_SYSTEM_FOCUSED)
                        {
                            flag = true;
                        }
                    }
                    return flag;
                });
            }
        }
    }
    return flag;
}

#endif // IACCESSIBLEUTILS_H_
