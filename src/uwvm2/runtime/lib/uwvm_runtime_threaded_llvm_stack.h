// Preserve native ELF macros while using LLVM enumerators in this member fragment.
#pragma push_macro("SHT_RELA")
#pragma push_macro("R_X86_64_64")
#undef SHT_RELA
#undef R_X86_64_64
// PRIVATE actual generated-object fixed-frame evidence. SOURCE ONLY.
// This does NOT certify libLLVM compiler-worker or Native import call chains.
        struct native_frame_record
        {
            ::std::string name;
            ::std::uint64_t section{UINT64_MAX},offset{UINT64_MAX},bytes{},object_bytes{UINT64_MAX};
        };
        ::std::vector<native_frame_record> native_frames_{};
        ::std::size_t maximum_fixed_frame_{},guest_fixed_stack_charge_{};
        ::std::size_t guest_native_stack_usable_bytes_{},compiler_native_stack_usable_bytes_{};
        bool machine_stack_valid_{true};
        // This pass is inserted immediately BEFORE the trusted x86 printer in
        // THIS private native pipeline. It observes final, post-PEI machine
        // frames; it neither changes ordinary pipelines nor estimates from IR.
        class machine_stack_observer final:public ::llvm::MachineFunctionPass
        {
            ThreadedLlvmChild& owner_;
            inline static char pass_id_{};
        public:
            explicit machine_stack_observer(ThreadedLlvmChild& owner):MachineFunctionPass(pass_id_),owner_{owner}{}
            ::llvm::StringRef getPassName() const override{return "UWVM threaded final machine frame witness";}
            void getAnalysisUsage(::llvm::AnalysisUsage& usage) const override
            {MachineFunctionPass::getAnalysisUsage(usage);usage.setPreservesAll();}
            bool runOnMachineFunction(::llvm::MachineFunction& function) override
            {
                auto& owner=owner_;auto const& frame=function.getFrameInfo();
                auto const* registers=function.getSubtarget().getRegisterInfo();
                auto const* lowering=function.getSubtarget().getFrameLowering();
                auto const cap=owner.entries_.size()*2u+256u;
                auto const name=function.getName();
                if(!owner.machine_stack_valid_)return false;
                if(registers==nullptr||lowering==nullptr||name.empty()||name.size()>256u||
                   !function.getProperties().hasProperty(::llvm::MachineFunctionProperties::Property::NoVRegs)||
                   !function.getFunction().hasFnAttribute(::llvm::Attribute::NoRedZone)||
                   frame.hasVarSizedObjects()||frame.hasOpaqueSPAdjustment()||frame.getUnsafeStackSize()!=0u||
                   frame.getMaxAlign().value()>16u||
                   registers->hasStackRealignment(function)||!lowering->hasReservedCallFrame(function)||
                   owner.native_frames_.size()==cap)
                {owner.machine_stack_valid_=false;return false;}
                for(auto const& old:owner.native_frames_)if(old.name==name)
                {owner.machine_stack_valid_=false;return false;}
                owner.native_frames_.push_back({name.str(),UINT64_MAX,UINT64_MAX,frame.getStackSize(),UINT64_MAX});
                return false;
            }
        };
        class observed_native_pass_manager final:public ::llvm::legacy::PassManager
        {
            ThreadedLlvmChild& owner_;
            ::std::size_t printers_{};
        public:
            explicit observed_native_pass_manager(ThreadedLlvmChild& owner):owner_{owner}{}
            void add(::llvm::Pass* pass) override
            {
                // Exact source-bound LLVM x86 pipeline stage, not a Guest name
                // or external callback authority. An unknown/renamed pipeline
                // fails coverage; no metadata-only success is manufactured.
                if(pass->getPassName()=="X86 Assembly Printer")
                {++printers_;::llvm::legacy::PassManager::add(new machine_stack_observer{owner_});}
                ::llvm::legacy::PassManager::add(pass);
            }
            [[nodiscard]] bool valid() const noexcept
            {return printers_==1u&&owner_.machine_stack_valid_&&!owner_.native_frames_.empty();}
        };
        [[nodiscard]] bool check_static_stack_ir(::llvm::Module& module) noexcept
        {
            for(auto& function:module)
            {
                if(function.isDeclaration())continue;
                function.addFnAttr(::llvm::Attribute::NoRedZone);
                if(function.hasFnAttribute(::llvm::Attribute::Naked)||function.hasFnAttribute("split-stack"))return false;
                for(auto& block:function)for(auto& instruction:block)
                {
                    if(auto alloca=::llvm::dyn_cast<::llvm::AllocaInst>(&instruction))
                        if(!alloca->isStaticAlloca()||alloca->getAlign().value()>16u)return false;
                    if(auto call=::llvm::dyn_cast<::llvm::CallBase>(&instruction))
                    {
                        if(call->hasFnAttr(::llvm::Attribute::ReturnsTwice))return false;
                        auto callee=call->getCalledFunction();if(callee==nullptr)continue;
                        auto name=callee->getName();
                        if(name.starts_with("llvm.stacksave")||name.starts_with("llvm.stackrestore")||
                           name.starts_with("llvm.coro.")||name.starts_with("llvm.localescape")||
                           name.starts_with("llvm.localrecover"))return false;
                    }
                }
            }
            return true;
        }
        [[nodiscard]] bool check_static_stack_object(::llvm::object::ObjectFile const& object)
        {
            if(!machine_stack_valid_||native_frames_.empty())return false;
            auto cap=entries_.size()*2u+256u;
            ::std::size_t actual_functions{};
            for(auto const& symbol:object.symbols())
            {
                auto flags=symbol.getFlags();if(!flags){::llvm::consumeError(flags.takeError());return false;}
                if(*flags&::llvm::object::SymbolRef::SF_Undefined)continue;
                auto type=symbol.getType();if(!type){::llvm::consumeError(type.takeError());return false;}
                if(*type!=::llvm::object::SymbolRef::ST_Function)continue;
                auto name=symbol.getName();auto section=symbol.getSection();auto value=symbol.getValue();
                if(!name){::llvm::consumeError(name.takeError());return false;}
                if(!section){::llvm::consumeError(section.takeError());return false;}
                if(!value){::llvm::consumeError(value.takeError());return false;}
                if(name->empty()||name->size()>256u||*section==object.section_end()||!(*section)->isText()||++actual_functions>cap)return false;
                native_frame_record* found{};
                for(auto& frame:native_frames_)if(frame.name==*name)
                {if(found!=nullptr||frame.section!=UINT64_MAX)return false;found=::std::addressof(frame);}
                if(found==nullptr)return false; // Every actual body must have a final MFI witness.
                found->section=(*section)->getIndex();found->offset=*value;
            }
            if(actual_functions!=native_frames_.size())return false;
            for(auto const& frame:native_frames_)if(frame.section==UINT64_MAX)return false;
            ::std::size_t records{};
            for(auto const& section:object.sections())
            {
                auto name=section.getName();if(!name){::llvm::consumeError(name.takeError());return false;}
                if(*name!=".stack_sizes")continue;
                auto data=section.getContents();if(!data){::llvm::consumeError(data.takeError());return false;}
                struct raw_record{::std::uint64_t offset{},bytes{};bool relocated{};};
                ::std::vector<raw_record> parsed;
                ::std::size_t at{};
                while(at<data->size())
                {
                    auto start=at;if(data->size()-at<8u)return false;
                    // This x86_64 RELA profile requires an unrelocated zero field;
                    // the exact function identity comes from symbol+addend below.
                    for(unsigned i{};i<8u;++i)if((*data)[at+i]!=0)return false;
                    at+=8u;::std::uint64_t bytes{};unsigned shift{};bool ended{};
                    for(unsigned i{};i<10u&&at<data->size();++i)
                    {
                        auto value=static_cast<unsigned char>((*data)[at++]);
                        if(shift==63u&&(value&0x7fu)>1u)return false;
                        bytes|=static_cast<::std::uint64_t>(value&0x7fu)<<shift;
                        if(!(value&0x80u)){ended=true;break;}shift+=7u;
                    }
                    if(!ended||bytes>SIZE_MAX||++records>cap)return false;
                    parsed.push_back({start,bytes,false});
                }
                ::std::size_t relocation_sections{};
                for(auto const& relocation_section:object.sections())
                {
                    auto target=relocation_section.getRelocatedSection();
                    if(!target){::llvm::consumeError(target.takeError());return false;}
                    if(*target==object.section_end()||(*target)->getIndex()!=section.getIndex())continue;
                    if(++relocation_sections!=1u||::llvm::object::ELFSectionRef{relocation_section}.getType()!=::llvm::ELF::SHT_RELA)return false;
                    for(auto const& relocation:relocation_section.relocations())
                    {
                        if(relocation.getType()!=::llvm::ELF::R_X86_64_64)return false;
                        raw_record* record{};
                        for(auto& candidate:parsed)if(candidate.offset==relocation.getOffset())
                        {if(record!=nullptr)return false;record=&candidate;}
                        if(record==nullptr||record->relocated)return false;
                        auto symbol=relocation.getSymbol();if(symbol==object.symbol_end())return false;
                        auto symbol_section=symbol->getSection();auto value=symbol->getValue();
                        auto addend=::llvm::object::ELFRelocationRef{relocation}.getAddend();
                        if(!symbol_section){::llvm::consumeError(symbol_section.takeError());return false;}
                        if(!value){::llvm::consumeError(value.takeError());return false;}
                        if(!addend){::llvm::consumeError(addend.takeError());return false;}
                        if(*symbol_section==object.section_end()||!(*symbol_section)->isText()||*addend<0||
                           static_cast<::std::uint64_t>(*addend)>UINT64_MAX-*value)return false;
                        auto offset=*value+static_cast<::std::uint64_t>(*addend);
                        bool found{};
                        for(auto& frame:native_frames_)if(frame.section==(*symbol_section)->getIndex()&&frame.offset==offset)
                        {
                            if(frame.object_bytes!=UINT64_MAX||frame.bytes!=record->bytes)return false;
                            frame.object_bytes=record->bytes;found=true;
                        }
                        if(!found)return false;record->relocated=true;
                    }
                }
                if(relocation_sections!=1u)return false;
                for(auto const& record:parsed)if(!record.relocated)return false;
            }
            maximum_fixed_frame_=0u;
            for(auto const& frame:native_frames_)
            {
                if(frame.object_bytes==UINT64_MAX||frame.bytes!=frame.object_bytes||frame.bytes>SIZE_MAX)return false;
                maximum_fixed_frame_=(::std::max)(maximum_fixed_frame_,static_cast<::std::size_t>(frame.bytes));
            }
            // SysV x86_64 CALL pushes exactly 8 bytes. No red zone, dynamic
            // realignment, dynamic alloca, unsafe stack or unreserved outgoing
            // call-frame shape survived the actual final-machine observer.
            // enter() rejects depth > limit only after that callee's prologue:
            // charge limit+1, including its unsuccessful entry. Native Host/
            // run_entry/compiler stacks remain SEPARATE, unproven boundaries.
            constexpr ::std::size_t return_address=8u;
            if(maximum_fixed_frame_>SIZE_MAX-return_address||call_limit_==SIZE_MAX||
               maximum_fixed_frame_+return_address>SIZE_MAX/(call_limit_+1u))return false;
            guest_fixed_stack_charge_=(maximum_fixed_frame_+return_address)*(call_limit_+1u);
            auto const& limits=world_->admission().limits();
            return guest_fixed_stack_charge_<=limits.max_value_stack_bytes&&guest_native_stack_usable_bytes_!=0u&&
                guest_fixed_stack_charge_<=guest_native_stack_usable_bytes_;
        }
#pragma pop_macro("R_X86_64_64")
#pragma pop_macro("SHT_RELA")
