// Bounded data-only split tree. Coordinates are terminal cells, never host pointers.
#pragma once
#include <array>
#include <algorithm>
#include <cstddef>
#include <span>
#include <utility>
#include <fast_io.h>
#include <fast_io_dsal/string.h>
#include <fast_io_dsal/string_view.h>
namespace uwvm2::uwvm::debugger::console_layout
{
    using view=::fast_io::string_view;
    [[nodiscard]] inline constexpr view pane_alias(view name) noexcept
    { return name=="stack" ? view{"operands"} : name=="register" || name=="registers" ? view{"regs"} : name; }
    struct node { unsigned pane{10u},first{},second{},ratio{50u}; bool horizontal{},leaf{true}; unsigned numerator{},denominator{}; };
    struct rectangle { unsigned pane{};::std::size_t left{},top{},width{},height{};unsigned node_index{}; };
    struct tree
    {
        ::std::array<node,15u> nodes{}; unsigned count{},root{};
        inline static constexpr view names[]{"src","asm","split","regs","wasm","wasip1","memory","operands","locals","io"};
        static unsigned id(view name) noexcept
        { name=pane_alias(name);for(unsigned i{};i<10u;++i) { if(name==names[i] && i!=2u) { return i; } }return 10u; }
        unsigned add(node n) noexcept { if(count==nodes.size()) { return 15u; }nodes[count]=n;return count++; }
        void chain(::std::span<unsigned const> panes,bool horizontal)
        {
            count=0u;if(panes.empty()) { return; }root=add({panes.front()});
            for(unsigned i{1u};i<panes.size();++i) { auto leaf{add({panes[i]})};root=add({10u,root,leaf,static_cast<unsigned>(100u*i/(i+1u)),horizontal,false,i,i+1u}); }
        }
        ::std::pair<::std::size_t,::std::size_t> minimum(unsigned at) const noexcept
        { auto const& n{nodes[at]};if(n.leaf) { return {n.pane==6u ? 32u : 20u,3u}; }auto a{minimum(n.first)},b{minimum(n.second)};
          return n.horizontal ? ::std::pair{a.first+b.first,::std::max(a.second,b.second)} : ::std::pair{::std::max(a.first,b.first),a.second+b.second}; }
        template<typename Visitor> void visit(unsigned at,rectangle box,Visitor&& visitor) const
        {
            auto const& n{nodes[at]};if(n.leaf) { box.pane=n.pane;box.node_index=at;visitor(box);return; }
            auto const a{minimum(n.first)},b{minimum(n.second)};auto const extent{n.horizontal ? box.width : box.height};
            auto const low{n.horizontal ? a.first : a.second},high{n.horizontal ? b.first : b.second};
            if(extent<low+high) { return; }auto const split{::std::clamp(n.denominator ? extent*n.numerator/n.denominator : extent*n.ratio/100u,low,extent-high)};
            auto left{box},right{box};if(n.horizontal) { left.width=split;right.left+=split;right.width-=split; }
            else { left.height=split;right.top+=split;right.height-=split; }
            visit(n.first,left,visitor);visit(n.second,right,visitor);
        }
        ::fast_io::string serialize(unsigned at) const
        { auto const& n{nodes[at]};if(n.leaf) { return ::fast_io::concat_fast_io(names[n.pane]); }
          auto ratio{n.denominator ? ::fast_io::concat_fast_io(n.numerator,"/",n.denominator) : ::fast_io::concat_fast_io(n.ratio)};
          return ::fast_io::concat_fast_io(n.horizontal ? view{"h"} : view{"v"},ratio,"(",serialize(n.first),",",serialize(n.second),")"); }
        ::fast_io::string serialize() const { return count ? serialize(root) : ::fast_io::string{}; }
        bool parse(view input)
        {
            tree next{};unsigned seen{};
            auto parse_node{[&](auto&& self)->unsigned
            {
                if(next.count==15u || input.empty()) { return 15u; }
                if((input.front()=='h' || input.front()=='v') && input.size()>1u && input[1]>='0' && input[1]<='9')
                {
                    bool horizontal{input.front()=='h'};input=input.subview(1u);unsigned ratio{},digits{};
                    while(!input.empty() && input.front()>='0' && input.front()<='9' && digits++<3u) { ratio=ratio*10u+static_cast<unsigned>(input.front()-'0');input=input.subview(1u); }
                    unsigned numerator{},denominator{};
                    if(!input.empty() && input.front()=='/')
                    { numerator=ratio;input=input.subview(1u);digits=0u;
                      while(!input.empty() && input.front()>='0' && input.front()<='9' && digits++<3u)
                      { denominator=denominator*10u+static_cast<unsigned>(input.front()-'0');input=input.subview(1u); }
                      if(!numerator || denominator<=numerator || denominator>999u) { return 15u; }
                      ratio=::std::clamp(numerator*100u/denominator,1u,99u); }
                    if(ratio<1u || ratio>99u || input.empty() || input.front()!='(') { return 15u; }input=input.subview(1u);
                    auto first{self(self)};if(first==15u || input.empty() || input.front()!=',') { return 15u; }input=input.subview(1u);
                    auto second{self(self)};if(second==15u || input.empty() || input.front()!=')') { return 15u; }input=input.subview(1u);
                    return next.add({10u,first,second,ratio,horizontal,false,numerator,denominator});
                }
                ::std::size_t length{};while(length<input.size() && input[length]!=',' && input[length]!=')') { ++length; }
                auto pane{id(input.subview(0u,length))};if(pane==10u || (seen&(1u<<pane))) { return 15u; }
                seen|=1u<<pane;input=input.subview(length);return next.add({pane});
            }};
            next.root=parse_node(parse_node);if(next.root==15u || !input.empty()) { return false; }*this=next;return true;
        }
        bool move(unsigned pane,unsigned target,view side)
        {
            if(pane==target || pane>=10u || target>=10u || (side!="left" && side!="right" && side!="above" && side!="below")) { return false; }
            bool source_found{},target_found{};for(unsigned i{};i<count;++i) { if(nodes[i].leaf) { source_found|=nodes[i].pane==pane;target_found|=nodes[i].pane==target; } }
            if(!source_found || !target_found) { return false; }
            tree next{};
            auto copy{[&](auto&& self,unsigned at)->unsigned
            { auto n{nodes[at]};if(n.leaf) { if(n.pane==pane) { return 15u; }auto copied{next.add(n)};
                if(n.pane==target) { auto moved{next.add({pane})};bool before{side=="left" || side=="above"};return next.add({10u,before ? moved : copied,before ? copied : moved,50u,side=="left" || side=="right",false}); }return copied; }
              auto first{self(self,n.first)},second{self(self,n.second)};if(first==15u) { return second; }if(second==15u) { return first; }
              n.first=first;n.second=second;return next.add(n); }};
            next.root=copy(copy,root);if(next.root==15u) { return false; }*this=next;return true;
        }
        bool remove(unsigned pane)
        {
            if(!count || nodes[root].leaf) { return false; }tree next{};bool found{};
            auto copy{[&](auto&& self,unsigned at)->unsigned
            { auto n{nodes[at]};if(n.leaf) { if(n.pane==pane) { found=true;return 15u; }return next.add(n); }
              auto first{self(self,n.first)},second{self(self,n.second)};if(first==15u) { return second; }if(second==15u) { return first; }
              n.first=first;n.second=second;return next.add(n); }};
            next.root=copy(copy,root);if(!found || next.root==15u) { return false; }*this=next;return true;
        }
        bool insert(unsigned pane,unsigned target,view side)
        {
            if(pane>=10u || pane==2u || count>13u || (side!="left" && side!="right" && side!="above" && side!="below")) { return false; }
            unsigned at{15u};for(unsigned i{};i<count;++i) { if(nodes[i].leaf) { if(nodes[i].pane==pane) { return false; }if(nodes[i].pane==target) { at=i; } } }
            if(at==15u) { return false; }auto existing{add(nodes[at])},created{add({pane})};bool before{side=="left" || side=="above"};
            nodes[at]={10u,before ? created : existing,before ? existing : created,50u,side=="left" || side=="right",false};return true;
        }
        // Resize the immediate parent split; every subtree retains its minimum.
        bool resize(unsigned pane,::std::size_t desired,rectangle bounds)
        {
            bool changed{};
            auto walk{[&](auto&& self,unsigned at,rectangle box)->void
            { auto& n{nodes[at]};if(n.leaf) { return; }auto a{minimum(n.first)},b{minimum(n.second)};
              auto extent{n.horizontal ? box.width : box.height},low{n.horizontal ? a.first : a.second},high{n.horizontal ? b.first : b.second};
              if(extent<low+high) { return; }auto split{::std::clamp(n.denominator ? extent*n.numerator/n.denominator : extent*n.ratio/100u,low,extent-high)};
              bool left{nodes[n.first].leaf && nodes[n.first].pane==pane},right{nodes[n.second].leaf && nodes[n.second].pane==pane};
              if(left || right) { auto requested{left ? desired : extent-::std::min(desired,extent)};split=::std::clamp(requested,low,extent-high);n.ratio=static_cast<unsigned>(::std::clamp((split*100u+extent/2u)/extent,::std::size_t{1u},::std::size_t{99u}));n.numerator=static_cast<unsigned>(split);n.denominator=static_cast<unsigned>(extent);changed=true;return; }
              auto first{box},second{box};if(n.horizontal) { first.width=split;second.left+=split;second.width-=split; }else { first.height=split;second.top+=split;second.height-=split; }
              self(self,n.first,first);if(!changed) { self(self,n.second,second); } }};
            if(count) { walk(walk,root,bounds); }return changed;
        }
    };
}
