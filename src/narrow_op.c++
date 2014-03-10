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

#include "narrow_op.h++"

typedef std::vector<std::shared_ptr<libflo::operation<narrow_node>>> out_t;

out_t narrow_op(const std::shared_ptr<libflo::operation<wide_node>> op,
                size_t width)
{
    /* Check to see if this operation just fits within a machine word,
     * in which case we don't really have to do anything. */
    {
        bool too_large = false;
        for (auto it = op->operands(); !it.done(); ++it) {
            auto op = *it;
            if (op->width() > width)
                too_large = true;
        }

        /* If none of the operands were too large to fit within a
         * machine word then just do the cast. */
        if (too_large == false) {
            std::shared_ptr<narrow_node> d;
            std::vector<std::shared_ptr<narrow_node>> s;

            d = narrow_node::clone_from(op->d());
            for (auto it = op->sources(); !it.done(); ++it)
                s.push_back(narrow_node::clone_from(*it));

            auto ptr = libflo::operation<narrow_node>::create(d,
                                                              op->width_u(),
                                                              op->op(),
                                                              s);

            out_t out;
            out.push_back(ptr);
            return out;
        }
    }

    /* At this point we know the operation is too wide so we need to
     * remap it to some narrower operations. */
    out_t out;

    switch (op->op()) {
        /* These are bit-wise operations, which are basically just
         * replicated N times as there's no dependencies at all! */
    case libflo::opcode::AND:
    case libflo::opcode::MUX:
    case libflo::opcode::OUT:
    case libflo::opcode::REG:
    case libflo::opcode::XOR:
    {
        size_t i = 0;
        for (auto it = op->d()->nnodes(); !it.done(); ++i, ++it) {
            auto d = *it;

            std::vector<std::shared_ptr<narrow_node>> svec;
            for (auto it = op->sources(); !it.done(); ++it) {
                auto s = *it;

                /* This makes MUX work: the idea is that if the width
                 * is 1 (like MUX's select signal is) then we'll
                 * always pick the first node.  Note that this is
                 * perfectly fine in the general case because that's
                 * what would be picked anyway! */
                if (s->width() == 1)
                    svec.push_back(s->nnode(0));
                else
                    svec.push_back(s->nnode(i));
            }

            auto ptr = libflo::operation<narrow_node>::create(d,
                                                              d->width_u(),
                                                              op->op(),
                                                              svec);
            out.push_back(ptr);
        }

        break;
    }

    case libflo::opcode::ADD:
    case libflo::opcode::ARSH:
    case libflo::opcode::CAT:
    case libflo::opcode::CATD:
    case libflo::opcode::EAT:
    case libflo::opcode::EQ:
    case libflo::opcode::GTE:
    case libflo::opcode::IN:
    case libflo::opcode::LD:
    case libflo::opcode::LIT:
    case libflo::opcode::LOG2:
    case libflo::opcode::LSH:
    case libflo::opcode::LT:
    case libflo::opcode::MEM:
    case libflo::opcode::MOV:
    case libflo::opcode::MSK:
    case libflo::opcode::MUL:
    case libflo::opcode::NEG:
    case libflo::opcode::NEQ:
    case libflo::opcode::NOP:
    case libflo::opcode::NOT:
    case libflo::opcode::OR:
    case libflo::opcode::RD:
    case libflo::opcode::RND:
    case libflo::opcode::RSH:
    case libflo::opcode::RST:
    case libflo::opcode::ST:
    case libflo::opcode::SUB:
    case libflo::opcode::WR:
        fprintf(stderr, "Can't narrow operation '%s'\n",
                opcode_to_string(op->op()).c_str());
        abort();
    }

    /* We now need to CATD together a bunch of nodes such that they
     * produce exactly the same result as the wide operation would. */
    std::shared_ptr<narrow_node> prev = NULL;
    size_t i;
    for (i = 0; i < op->d()->nnode_size(); ++i) {
        if (prev == NULL) {
            prev = op->d()->catdnode(i);

            auto ptr = libflo::operation<narrow_node>
                ::create(prev,
                         prev->width_u(),
                         libflo::opcode::XOR,
                         {op->d()->nnode(0),
                                 op->d()->nnode(0)}
                    );
            out.push_back(ptr);
            continue;
        }

        auto next = op->d()->catdnode(i);
        auto ptr = libflo::operation<narrow_node>::create(next,
                                                          next->width_u(),
                                                          libflo::opcode::CATD,
                                                          {op->d()->nnode(i),
                                                                  prev}
            );
        out.push_back(ptr);
        prev = next;
    }

    return out;
}
