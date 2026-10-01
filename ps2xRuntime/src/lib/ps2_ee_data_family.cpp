#include "ps2_ee_data_family.h"
#include <cstring>
#include <set>
#include <stdexcept>
#include <utility>

namespace ps2native::ee_family
{
    Dispatcher::Dispatcher(const Program &program)
    {
        if(program.families.size()>512) throw std::invalid_argument("EE family catalog exceeds budget");
        std::set<std::vector<uint32_t>> identities;
        for(const auto &family:program.families)
        {
            if(!family.function || family.words.empty() || family.words.size()>128 ||
               family.words.size()!=family.masks.size() || family.normalOffsets.empty() ||
               family.normalOffsets.size()>family.words.size())
                throw std::invalid_argument("invalid EE family dimensions");
            Owned owned{family.function,{},{family.masks.begin(),family.masks.end()}};
            std::vector<uint32_t> identity;
            for(size_t i=0;i<family.words.size();++i)
            {
                const uint32_t word=family.words[i],mask=family.masks[i];
                if(mask!=0xffffffffu && (mask!=0xffff0000u ||
                   !((word>>26)==0x2bu || ((word>>26)==0x0fu && ((word>>21)&31u)==0u))))
                    throw std::invalid_argument("EE family mask changes instruction structure");
                owned.words.push_back(word&mask);identity.push_back(word&mask);identity.push_back(mask);
            }
            if(!identities.insert(identity).second)
                throw std::invalid_argument("duplicate EE family structure");
            std::set<uint32_t> offsets;
            const uint32_t index=static_cast<uint32_t>(m_families.size());
            for(const auto offset:family.normalOffsets)
            {
                if((offset&3u) || offset>=family.words.size()*4 || !offsets.insert(offset).second)
                    throw std::invalid_argument("invalid EE family normal entry");
                const size_t word=offset/4;
                auto &directory=owned.masks[word]==0xffffffffu ? m_exact : m_upper;
                directory[owned.words[word]].push_back({index,offset});
            }
            m_families.push_back(std::move(owned));
        }
    }

    Admission Dispatcher::lookup(const uint8_t *ram,uint32_t pc) const
    {
        if(!ram) return {nullptr,0,Status::NoRam};
        if(pc&3u) return {nullptr,0,Status::MisalignedPc};
        if(pc>=PS2_RAM_SIZE) return {nullptr,0,Status::OutsideRam};
        uint32_t live;std::memcpy(&live,ram+pc,4);
        Admission result;
        const auto examine=[&](const auto &directory,uint32_t key)
        {
            const auto found=directory.find(key);
            if(found==directory.end()) return;
            for(const auto &entry:found->second)
            {
                ++result.candidatesChecked;
                const auto &family=m_families[entry.family];
                if(entry.offset>pc) continue;
                const uint32_t base=pc-entry.offset;
                if(base>PS2_RAM_SIZE-family.words.size()*4) continue;
                bool matches=true;
                for(size_t i=0;i<family.words.size();++i)
                {
                    uint32_t value;std::memcpy(&value,ram+base+i*4,4);
                    if((value&family.masks[i])!=family.words[i]) { matches=false;break; }
                }
                if(!matches) continue;
                if(result.function || result.status==Status::Ambiguous)
                {
                    result.function=nullptr;result.base=0;result.status=Status::Ambiguous;
                }
                else
                {
                    result.function=family.function;result.base=base;result.status=Status::Ready;
                }
            }
        };
        examine(m_exact,live);examine(m_upper,live&0xffff0000u);
        return result;
    }

    Admission Dispatcher::admitNormalEntry(const uint8_t *ram,const R5900Context *context) const
    {
        if(!context || context->in_delay_slot) return {nullptr,0,Status::UnsupportedEntryContext};
        return lookup(ram,context->pc);
    }
}
