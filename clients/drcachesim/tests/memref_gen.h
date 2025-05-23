/* **********************************************************
 * Copyright (c) 2021-2025 Google, LLC  All rights reserved.
 * **********************************************************/

/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of Google, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, LLC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef _MEMREF_GEN_
#define _MEMREF_GEN_ 1

#include "memref.h"
#include "dr_api.h"
#include <assert.h>
#include <cstring>
#include <vector>

namespace dynamorio {
namespace drmemtrace {

#ifdef X86
#    define REG1 DR_REG_XAX
#    define REG2 DR_REG_XDX
#elif defined(AARCHXX)
#    define REG1 DR_REG_R0
#    define REG2 DR_REG_R1
#elif defined(RISCV64)
#    define REG1 DR_REG_A0
#    define REG2 DR_REG_A1
#else
#    error Unsupported arch
#endif

struct memref_with_IR_t {
    memref_t memref;
    instr_t *instr; // This is an instr created with DR IR APIs and is set only
                    // for instrs created as such.
};

inline memref_t
gen_data_type(memref_tid_t tid, trace_type_t type, addr_t addr, size_t size = 1)
{
    memref_t memref = {};
    memref.data.type = type;
    memref.data.tid = tid;
    memref.data.addr = addr;
    memref.data.size = size;
    return memref;
}

inline memref_t
gen_data(memref_tid_t tid, bool load, addr_t addr, size_t size)
{
    trace_type_t type = load ? TRACE_TYPE_READ : TRACE_TYPE_WRITE;
    return gen_data_type(tid, type, addr, size);
}

inline memref_t
gen_instr_type(trace_type_t type, memref_tid_t tid, addr_t pc = 1, size_t size = 1,
               addr_t indirect_branch_target = 0, memref_pid_t pid = 0)
{
    memref_t memref = {};
    memref.instr.type = type;
    memref.instr.tid = tid;
    memref.instr.pid = pid;
    memref.instr.addr = pc;
    memref.instr.size = size;
    memref.instr.indirect_branch_target = indirect_branch_target;
    return memref;
}

inline memref_t
gen_instr(memref_tid_t tid, addr_t pc = 1, size_t size = 1, memref_pid_t pid = 0)
{
    return gen_instr_type(TRACE_TYPE_INSTR, tid, pc, size, /*indirect_branch_target=*/0,
                          pid);
}

inline memref_t
gen_branch(memref_tid_t tid, addr_t pc = 1)
{
    return gen_instr_type(TRACE_TYPE_INSTR_UNTAKEN_JUMP, tid, pc);
}

// We use these client defines which are the target and so drdecode's target arch.
#if defined(ARM_64) || defined(ARM_32)
// Variant for aarchxx encodings.
inline memref_t
gen_instr_encoded(addr_t pc, int encoding, memref_tid_t tid = 1)
{
    memref_t memref = gen_instr_type(TRACE_TYPE_INSTR, tid, pc, 4);
    memcpy(memref.instr.encoding, &encoding, sizeof(encoding));
    memref.instr.encoding_is_new = true;
    return memref;
}

inline memref_t
gen_branch_encoded(memref_tid_t tid, addr_t pc, int encoding)
{
    memref_t memref = gen_instr_type(TRACE_TYPE_INSTR_CONDITIONAL_JUMP, tid, pc);
    memref.instr.size = 4;
    memcpy(memref.instr.encoding, &encoding, sizeof(encoding));
    memref.instr.encoding_is_new = true;
    return memref;
}

#elif defined(X86_64) || defined(X86_32)
inline memref_t
gen_instr_encoded(addr_t pc, const std::vector<char> &encoding, memref_tid_t tid = 1)
{
    memref_t memref = gen_instr_type(TRACE_TYPE_INSTR, tid, pc, encoding.size());
    memcpy(memref.instr.encoding, encoding.data(), encoding.size());
    memref.instr.encoding_is_new = true;
    return memref;
}

// Variant for x86 encodings.
inline memref_t
gen_branch_encoded(memref_tid_t tid, addr_t pc, const std::vector<char> &encoding)
{
    memref_t memref = gen_instr_type(TRACE_TYPE_INSTR_CONDITIONAL_JUMP, tid, pc);
    memref.instr.size = encoding.size();
    memcpy(memref.instr.encoding, encoding.data(), encoding.size());
    memref.instr.encoding_is_new = true;
    return memref;
}
#else
#endif

inline memref_t
gen_marker(memref_tid_t tid, trace_marker_type_t type, uintptr_t val)
{
    memref_t memref = {};
    memref.marker.type = TRACE_TYPE_MARKER;
    memref.marker.tid = tid;
    memref.marker.marker_type = type;
    memref.marker.marker_value = val;
    return memref;
}

inline memref_t
gen_exit(memref_tid_t tid)
{
    memref_t memref = {};
    memref.exit.type = TRACE_TYPE_THREAD_EXIT;
    memref.exit.tid = tid;
    return memref;
}

/* Returns a vector of memref_t with instr encodings.
 * For memref_with_IR_t, the caller has to set tid + pid fields of the memref_t in
 * memref_with_IR_t structs but not the other fields. For other memrefs the caller
 * should still set everything they need. Also note that all data memrefs have to
 * be filled in for each instr when constructing memref_instr_vec. Each instr
 * field in memref_instr_vec's elements needs to be constructed using DR's IR
 * API for creating instructions. Any PC-relative instr in ilist is encoded as
 * though the final instruction list were located at base_addr.
 * Markers with instr fields will have their values replaced with the instr's PC.
 * TRACE_MARKER_TYPE_BRANCH_TARGET markers specified in the input ilist are not
 * included in the returned memref_t list, but if an instr_t* is specified with
 * those markers, its pc is added to the next indirect branch instr's
 * memref_t.instr.indirect_branch_target field.
 */
std::vector<memref_t>
add_encodings_to_memrefs(instrlist_t *ilist,
                         std::vector<memref_with_IR_t> &memref_instr_vec,
                         addr_t base_addr, bool skip_encoding = false)
{
    static const int MAX_DECODE_SIZE = 2048;
    byte decode_buf[MAX_DECODE_SIZE];
#ifndef NDEBUG
    byte *pc =
#endif
        instrlist_encode_to_copy(GLOBAL_DCONTEXT, ilist, decode_buf,
                                 reinterpret_cast<app_pc>(base_addr), nullptr, true);
    assert(pc != nullptr);
    assert(pc <= decode_buf + sizeof(decode_buf));
    std::vector<memref_t> memrefs = {};
    addr_t next_indirect_branch_target = 0;
    for (auto pair : memref_instr_vec) {
        bool skip = false;
        if (type_is_instr(pair.memref.instr.type)) {
            assert(pair.instr != nullptr);
            const size_t offset = instr_get_offset(pair.instr);
            const int instr_size = instr_length(GLOBAL_DCONTEXT, pair.instr);
            pair.memref.instr.addr = offset + base_addr;
            pair.memref.instr.size = instr_size;
            if (type_is_instr_branch(pair.memref.instr.type) &&
                !type_is_instr_direct_branch(pair.memref.instr.type)) {
                pair.memref.instr.indirect_branch_target = next_indirect_branch_target;
                next_indirect_branch_target = 0;
            }
            if (!skip_encoding) {
                memcpy(pair.memref.instr.encoding, &decode_buf[offset], instr_size);
                pair.memref.instr.encoding_is_new = true;
            }
        } else if (pair.memref.marker.type == TRACE_TYPE_MARKER) {
            if (pair.memref.marker.marker_type == TRACE_MARKER_TYPE_BRANCH_TARGET) {
                // TRACE_MARKER_TYPE_BRANCH_TARGET markers are not presented in the
                // memref_t stream.
                skip = true;
                if (pair.instr != nullptr) {
                    next_indirect_branch_target =
                        instr_get_offset(pair.instr) + base_addr;
                }
            } else if (pair.instr != nullptr) {
                pair.memref.marker.marker_value =
                    instr_get_offset(pair.instr) + base_addr;
            }
        } else
            assert(pair.instr == nullptr);
        if (!skip) {
            memrefs.push_back(pair.memref);
        }
    }
    return memrefs;
}

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _MEMREF_GEN_ */
