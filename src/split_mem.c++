/*
 * Copyright (C) 2014 Palmer Dabbelt
 *   <palmer.dabbelt@eecs.berkeley.edu>
 *
 * This file is part of flo-mwe.
 *
 * flo-mwe is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * flo-mwe is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with flo-mwe.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include "split_mem.h++"
#include <cmath>

typedef std::vector<std::shared_ptr<libflo::operation<shallow_node>>> out_t;

out_t split_mem(const std::shared_ptr<libflo::operation<narrow_node>> op,
                size_t depth)
{
    /* Most node won't be too deep.  In order to avoid screwing
     * anything up I just output those nodes directly. */
    {
        bool too_deep = false;
        for (auto it = op->operands(); !it.done(); ++it) {
            auto op = *it;
            if (op->depth() > depth)
                too_deep = true;
        }

        if (too_deep == false) {
            std::shared_ptr<shallow_node> d;
            std::vector<std::shared_ptr<shallow_node>> s;

            d = shallow_node::clone_from(op->d());
            for (auto it = op->sources(); !it.done(); ++it)
                s.push_back(shallow_node::clone_from(*it));

            out_t out;
            auto ptr = libflo::operation<shallow_node>::create(d,
                                                               op->width_u(),
                                                               op->op(),
                                                               s);
            out.push_back(ptr);
            return out;
        }
    }

    out_t out;
    switch (op->op()) {
    case libflo::opcode::RD:
    {
        auto addr = shallow_node::clone_from(op->u());

        size_t shift_count = std::log2(wide_node::get_mem_depth());
        size_t addr_lo_width = shift_count;
        size_t addr_hi_width = op->u()->width() - shift_count;

        auto addr_lo = shallow_node::create_temp(addr_lo_width);
        auto addr_lo_op = libflo::operation<shallow_node>::create(
            addr_lo,
            shift_count,
            libflo::opcode::RSH,
            {addr, shallow_node::create_const(addr, 0)}
            );
        out.push_back(addr_lo_op);

        auto addr_hi = shallow_node::create_temp(addr_hi_width);
        auto addr_hi_op = libflo::operation<shallow_node>::create(
            addr_hi,
            addr->width() - shift_count,
            libflo::opcode::RSH,
            {addr, shallow_node::create_const(addr, shift_count)}
            );
        out.push_back(addr_hi_op);

        /* FIXME: This generates a linear-tree of MUXes, which is
         * worse than the log-tree for large memories.  Note that this
         * is actually better for small memories (I think, at
         * least...?) and it's easier to write so I've stuck it in for
         * now. */
        size_t index = 0;
        auto prev_node = shallow_node::create_temp(op->d()->snode(0));

        for (auto it = op->t()->snodes(); !it.done(); ++it) {
            auto mem = *it;

            /* Load the memory into a new node. */
            auto new_node = shallow_node::create_temp(prev_node);
            auto load_op = libflo::operation<shallow_node>::create(
                new_node,
                new_node->width_u(),
                libflo::opcode::RD,
                {shallow_node::create_const(new_node, 1), mem, addr_lo}
                );
            out.push_back(load_op);

            /* If we're not the first node then check if there's a
             * match -- this means that there's a fall-through to the
             * first node when nothing matches. */
            if (index > 0) {
                auto index_const = shallow_node::create_const(addr_hi, index);

                auto index_match = shallow_node::create_temp(1);
                auto index_match_op = libflo::operation<shallow_node>::create(
                    index_match,
                    addr_hi->width_u(),
                    libflo::opcode::EQ,
                    {addr_hi, index_const}
                    );
                out.push_back(index_match_op);

                auto mux_node = shallow_node::create_temp(new_node);
                auto mux_op = libflo::operation<shallow_node>::create(
                    mux_node,
                    mux_node->width_u(),
                    libflo::opcode::MUX,
                    {index_match, prev_node, new_node}
                    );
                out.push_back(mux_op);
                new_node = mux_node;
            }

            /* Rotate to the new node set. */
            prev_node = new_node;
            index++;
        }

        auto d = op->d()->snode(0);
        auto mov_op = libflo::operation<shallow_node>::create(
            d,
            prev_node->width_u(),
            libflo::opcode::MOV,
            {prev_node}
            );
        out.push_back(mov_op);

        break;
    }

    case libflo::opcode::WR:
    {
        auto addr = shallow_node::clone_from(op->u());

        size_t shift_count = std::log2(wide_node::get_mem_depth());
        size_t addr_lo_width = shift_count;
        size_t addr_hi_width = op->u()->width() - shift_count;

        auto addr_lo = shallow_node::create_temp(addr_lo_width);
        auto addr_lo_op = libflo::operation<shallow_node>::create(
            addr_lo,
            shift_count,
            libflo::opcode::RSH,
            {addr, shallow_node::create_const(addr, 0)}
            );
        out.push_back(addr_lo_op);

        auto addr_hi = shallow_node::create_temp(addr_hi_width);
        auto addr_hi_op = libflo::operation<shallow_node>::create(
            addr_hi,
            addr->width() - shift_count,
            libflo::opcode::RSH,
            {addr, shallow_node::create_const(addr, shift_count)}
            );
        out.push_back(addr_hi_op);

        /* FIXME: This generates a linear-tree of MUXes, which is
         * worse than the log-tree for large memories.  Note that this
         * is actually better for small memories (I think, at
         * least...?) and it's easier to write so I've stuck it in for
         * now. */
        size_t index = 0;

        for (auto it = op->t()->snodes(); !it.done(); ++it) {
            auto mem = *it;

            auto index_const = shallow_node::create_const(addr_hi, index);

            auto index_match = shallow_node::create_temp(1);
            auto index_match_op = libflo::operation<shallow_node>::create(
                index_match,
                addr_hi->width_u(),
                libflo::opcode::EQ,
                {addr_hi, index_const}
                );
            out.push_back(index_match_op);

            auto wen = shallow_node::create_temp(1);
            auto wen_op = libflo::operation<shallow_node>::create(
                wen,
                wen->width_u(),
                libflo::opcode::AND,
                {index_match, op->s()->snode(0)}
                );
            out.push_back(wen_op);

            auto write_node = shallow_node::create_temp(op->d()->snode(0));
            auto write_op = libflo::operation<shallow_node>::create(
                write_node,
                write_node->width_u(),
                libflo::opcode::WR,
                {wen, mem, addr_lo, op->v()->snode(0)}
                );
            out.push_back(write_op);

            index++;
        }

        break;
    }

    case libflo::opcode::ADD:
    case libflo::opcode::AND:
    case libflo::opcode::ARSH:
    case libflo::opcode::CAT:
    case libflo::opcode::CATD:
    case libflo::opcode::EAT:
    case libflo::opcode::EQ:
    case libflo::opcode::GTE:
    case libflo::opcode::IN:
    case libflo::opcode::INIT:
    case libflo::opcode::LD:
    case libflo::opcode::LIT:
    case libflo::opcode::LOG2:
    case libflo::opcode::LSH:
    case libflo::opcode::LT:
    case libflo::opcode::MEM:
    case libflo::opcode::MOV:
    case libflo::opcode::MSK:
    case libflo::opcode::MUL:
    case libflo::opcode::MUX:
    case libflo::opcode::NEG:
    case libflo::opcode::NEQ:
    case libflo::opcode::NOP:
    case libflo::opcode::NOT:
    case libflo::opcode::OR:
    case libflo::opcode::OUT:
    case libflo::opcode::REG:
    case libflo::opcode::RND:
    case libflo::opcode::RSH:
    case libflo::opcode::RST:
    case libflo::opcode::ST:
    case libflo::opcode::SUB:
    case libflo::opcode::XOR:
        fprintf(stderr, "Can't shallow-ize operation '%s'\n",
                opcode_to_string(op->op()).c_str());
        abort();
    }
    return out;
}
