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
    case libflo::opcode::IN:
    case libflo::opcode::MOV:
    case libflo::opcode::MUX:
    case libflo::opcode::NOT:
    case libflo::opcode::OR:
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

        /* Addition requires a carry chain.  There's no Flo
         * instruction for carry chains, so I borrowed this algorithm
         * from the Hacker's Delight, Chapter 2 Part 16: Double-Length
         * Add/Subtract. */
    case libflo::opcode::ADD:
    case libflo::opcode::SUB:
    {
        /* Here we generate the carry bit, which is initially zero. */
        auto c = narrow_node::create_temp(op->s()->nnode(0));
        {
            auto c_op = libflo::operation<narrow_node>::create(
                c,
                c->width_u(),
                libflo::opcode::XOR,
                {op->s()->nnode(0), op->s()->nnode(0)}
                );
            out.push_back(c_op);
        }

        /* Walk through the D <= S + T node arrays, creating a sum at
         * each step and producing another carry bit. */
        for (size_t i = 0; i < op->d()->nnode_count(); ++i) {
            auto d = op->d()->nnode(i);
            auto s = op->s()->nnode(i);
            auto t = op->t()->nnode(i);

            /* The carry operation is really only a bit, so here we
             * just need to cast it to our width. */
            {
                auto nc = narrow_node::create_temp(s);
                auto z = narrow_node::create_temp(s);

                auto z_op = libflo::operation<narrow_node>::create(
                    z,
                    z->width_u(),
                    libflo::opcode::XOR,
                    {s, s}
                    );
                out.push_back(z_op);

                auto nc_op = libflo::operation<narrow_node>::create(
                    nc,
                    nc->width_u(),
                    libflo::opcode::RSH,
                    {c, z}
                    );
                out.push_back(nc_op);
                c = nc;
            }

            /* Compute the partial sum, which is just a simple
             * addition of the sources */
            auto partial = narrow_node::create_temp(d);
            {
                auto partial_op = libflo::operation<narrow_node>::create(
                    partial,
                    partial->width_u(),
                    op->op(),
                    {s, t}
                    );
                out.push_back(partial_op);
            }

            /* Compute the full sum (directly in place), which adds
             * the partial sum to the carry bit. */
            {
                auto full_op = libflo::operation<narrow_node>::create(
                    d,
                    d->width_u(),
                    op->op(),
                    {partial, c}
                    );
                out.push_back(full_op);
            }

            /* Compute a new carry bit.  Note that here I'm not using
             * a comparison because I don't want to run into -fwrapv
             * issues. */
            /* FIXME: Figure out if we can use LT */
            if (op->op() == libflo::opcode::ADD)
            {
                auto nc1 = narrow_node::create_temp(d);
                auto nc2 = narrow_node::create_temp(d);
                auto nc3 = narrow_node::create_temp(d);
                auto nc4 = narrow_node::create_temp(d);
                auto nc5 = narrow_node::create_temp(d);
                auto nc = narrow_node::create_temp(d);

                auto nc1_op = libflo::operation<narrow_node>::create(
                    nc1,
                    nc1->width_u(),
                    libflo::opcode::AND,
                    {s, t}
                    );
                out.push_back(nc1_op);

                auto nc2_op = libflo::operation<narrow_node>::create(
                    nc2,
                    nc2->width_u(),
                    libflo::opcode::OR,
                    {s, t}
                    );
                out.push_back(nc2_op);

                auto nc3_op = libflo::operation<narrow_node>::create(
                    nc3,
                    nc3->width_u(),
                    libflo::opcode::NOT,
                    {d}
                    );
                out.push_back(nc3_op);

                auto nc4_op = libflo::operation<narrow_node>::create(
                    nc4,
                    nc4->width_u(),
                    libflo::opcode::AND,
                    {nc2, nc3}
                    );
                out.push_back(nc4_op);

                auto nc5_op = libflo::operation<narrow_node>::create(
                    nc5,
                    nc5->width_u(),
                    libflo::opcode::OR,
                    {nc1, nc4}
                    );
                out.push_back(nc5_op);

                auto shift_width = narrow_node::create_const(d, d->width() - 1);
                auto nc_op = libflo::operation<narrow_node>::create(
                    nc,
                    nc->width_u(),
                    libflo::opcode::RSH,
                    {nc5, shift_width}
                    );
                out.push_back(nc_op);

                /* The whole purpose of this was to generate a new
                 * carry bit, which is now node here. */
                c = nc;
            }
            /* Subtract is safe from fwrapv troubles...  Note that the
             * version prescribed in the Hacker's Delight only works
             * for double-word subtraction, this is significantly
             * enhanced... */
            else if (op->op() == libflo::opcode::SUB)
            {
                auto zero = narrow_node::create_const(d, 0);

                /* This is true IFF the current operation ends up
                 * producing a carry. */
                auto carry_here = narrow_node::create_temp(d);
                auto carry_here_op = libflo::operation<narrow_node>::create(
                    carry_here,
                    carry_here->width_u(),
                    libflo::opcode::LT,
                    {s, t}
                    );
                out.push_back(carry_here_op);

                auto carry_here_w = narrow_node::create_temp(d);
                auto carry_here_w_op = libflo::operation<narrow_node>::create(
                    carry_here_w,
                    carry_here_w->width_u(),
                    libflo::opcode::RSH,
                    {carry_here, zero}
                    );
                out.push_back(carry_here_w_op);

                /* Here's the special case: if the old carry bit was
                 * set AND the sources are equal THEN we need to
                 * propogate the carry bit along. */

                /* First the equality test. */
                auto is_zero = narrow_node::create_temp(d);
                auto is_zero_op = libflo::operation<narrow_node>::create(
                    is_zero,
                    is_zero->width_u(),
                    libflo::opcode::EQ,
                    {s, t}
                    );
                out.push_back(is_zero_op);

                auto is_zero_w = narrow_node::create_temp(d);
                auto is_zero_w_op = libflo::operation<narrow_node>::create(
                    is_zero_w,
                    is_zero_w->width_u(),
                    libflo::opcode::RSH,
                    {is_zero, zero}
                    );
                out.push_back(is_zero_w_op);

                /* Equaliy isn't sufficient for a carry, we also need
                 * the old carry bit to be TRUE (otherwise it's just
                 * zero). */
                auto old_carry_w = narrow_node::create_temp(d);
                auto old_carry_w_op = libflo::operation<narrow_node>::create(
                    old_carry_w,
                    old_carry_w->width_u(),
                    libflo::opcode::AND,
                    {is_zero_w, c}
                    );
                out.push_back(old_carry_w_op);

                /* The new carry is just an OR of these two: either
                 * the old carry bit required a propogation or this
                 * required a new carry bit. */
                auto nc = narrow_node::create_temp(d);
                auto nc_op = libflo::operation<narrow_node>::create(
                    nc,
                    nc->width_u(),
                    libflo::opcode::OR,
                    {carry_here_w, old_carry_w}
                    );
                out.push_back(nc_op);

                /* The whole purpose of this was to generate a new
                 * "carry" (actually borrow) bit, which is now node
                 * here. */
                c = nc;
            }
            else
                abort();
        }

        break;
    }

        /* Shift operations*/
    case libflo::opcode::RSH:
    {
        /* We only support constant-offset shifts. */
        if (op->t()->is_const() == false) {
            fprintf(stderr, "Only constant-offset shifts are supported\n");
            abort();
        } else {

            for (size_t i = 0; i < op->d()->nnode_count(); ++i) {
                auto d = op->d()->nnode(i);

                size_t offset = op->t()->const_int();

                /* The RSH node performs a shift and a width
                 * modification.  This makes mapping it to short nodes
                 * particularly tricky, as sometimes the width will be
                 * really short. */
                auto width = op->width();
                if (width > wide_node::get_word_length())
                    width = wide_node::get_word_length();

                /* This calculates at which bit of the original,
                 * un-split word these bits will be coming from. */
                auto lo_bit = (i * wide_node::get_word_length()) + offset;
                auto hi_bit = lo_bit + width - 1;

                /* This calculates at which of the split words the
                 * data will be coming from. */
                auto lo_word = lo_bit / wide_node::get_word_length();
                auto hi_word = hi_bit / wide_node::get_word_length();

                /* This calculates the offsets into each word where
                 * bits should be fetched from. */
                auto lo_off = lo_bit % wide_node::get_word_length();
                auto hi_off = (hi_bit + 1) % wide_node::get_word_length();
                auto lo_offn = narrow_node::create_const(d, lo_off);
                auto hi_offn = narrow_node::create_const(d, 0);

                /* Some shifts are narrow and close enough that they
                 * can be satisfied by a single word-length operation,
                 * while others can't be. */
                if (lo_word == hi_word) {
                    auto word = lo_word;

                    if (word >= op->s()->nnode_count()) {
                        auto mov_op = libflo::operation<narrow_node>::create(
                            d,
                            libflo::unknown<size_t>(),
                            libflo::opcode::MOV,
                            {narrow_node::create_const(d, 0)}
                            );
                        out.push_back(mov_op);
                    } else {
                        auto bits = narrow_node::create_const(d, lo_off);
                        auto rsh_op = libflo::operation<narrow_node>::create(
                            d,
                            d->width(),
                            libflo::opcode::RSH,
                            {op->s()->nnode(word), bits}
                            );
                    }
                } else {
                    auto lo_width = op->s()->nnode(i)->width() - lo_off;
                    auto lo_dat = narrow_node::create_temp(lo_width);
                    auto lo_op = libflo::operation<narrow_node>::create(
                        lo_dat,
                        lo_dat->width(),
                        libflo::opcode::RSH,
                        {op->s()->nnode(lo_word), lo_offn}
                        );
                    out.push_back(lo_op);

                    auto hi_width = d->width() - lo_width;
                    auto hi_dat = narrow_node::create_temp(hi_width);
                    if (hi_word < op->s()->nnode_count()) {
                        auto hi_op = libflo::operation<narrow_node>::create(
                            hi_dat,
                            hi_off,
                            libflo::opcode::RSH,
                            {op->s()->nnode(hi_word), hi_offn}
                            );
                        out.push_back(hi_op);
                    } else {
                        hi_dat = narrow_node::create_const(hi_dat, 0);
                    }

                    if (d->width() != lo_width) {
                        auto cat_op = libflo::operation<narrow_node>::create(
                            d,
                            d->width_u(),
                            libflo::opcode::CAT,
                            {hi_dat, lo_dat}
                            );
                        out.push_back(cat_op);
                    } else {
                        auto mov_op = libflo::operation<narrow_node>::create(
                            d,
                            d->width_u(),
                            libflo::opcode::MOV,
                            {lo_dat}
                            );
                        out.push_back(mov_op);
                    }
                }
            }
        }

        break;
    }

    case libflo::opcode::CAT:
    {
        auto t_bit = op->t()->width();

        for (size_t i = 0; i < op->d()->nnode_count(); ++i) {
            auto d = op->d()->nnode(i);

            auto lo_bit = (i + 0) * wide_node::get_word_length();
            auto hi_bit = (i + 1) * wide_node::get_word_length();
            if (hi_bit > op->d()->width())
                hi_bit = op->d()->width();

            auto si = i - op->t()->nnode_count();

            if ((lo_bit < t_bit) && (hi_bit < t_bit)) {
                /* The first part of a CAT can always be satisfied by
                 * MOV operations.*/
                auto mov_op = libflo::operation<narrow_node>::create(
                    d,
                    d->width_u(),
                    libflo::opcode::MOV,
                    {op->t()->nnode(i)}
                    );
                out.push_back(mov_op);
            } else if ((lo_bit >= t_bit) && (hi_bit > t_bit)) {
                /* Here's the top half of a CAT, where everything is
                 * coming from the high node. */
                if (hi_bit >= op->d()->width()) {
                    if (si >= op->s()->nnode_count()) {
                        fprintf(stderr, "si too large: %lu in ", si);
                        op->writeln_debug(stderr);
                        abort();
                    }

                    /* A special case: we're at the highest word.
                     * This means there's nothing left to do but
                     * simply copy into place the extra bits. */
                    auto lo_bits = op->s()->nnode(si)->width() - d->width();
                    auto lo_bitsc = narrow_node::create_const(d, lo_bits);

                    auto rsh_op = libflo::operation<narrow_node>::create(
                        d,
                        d->width_u(),
                        libflo::opcode::RSH,
                        {op->s()->nnode(si), lo_bitsc}
                        );
                    out.push_back(rsh_op);
                } else {
                    fprintf(stderr, "Multi-word cat high not implemented 2\n");
                    op->writeln_debug(stderr);
                    abort();
                }
            } else {
                /* This is the middle part of a CAT, which is the only
                 * part that's taking data from both of the sources.
                 * It's effectively the same as a RSH, but with the
                 * extra wrinkle that neither of the widths are full
                 * words. */
                auto max_width = wide_node::get_word_length();
                auto trunc_width = op->s()->width();
                auto lo_width = op->t()->nnode(i)->width();
                if (trunc_width + lo_width > max_width)
                    trunc_width = max_width - lo_width;

                auto trunc = narrow_node::create_temp(trunc_width);

                if (op->s()->is_const()) {
                    auto trunc_op = libflo::operation<narrow_node>::create(
                        trunc,
                        trunc->width(),
                        libflo::opcode::OR,
                        {op->s()->nnode(0), op->s()->nnode(0)}
                        );
                    out.push_back(trunc_op);
                } else {
                    auto trunc_op = libflo::operation<narrow_node>::create(
                        trunc,
                        trunc->width(),
                        libflo::opcode::RSH,
                        {op->s()->nnode(0), narrow_node::create_const(trunc, 0)}
                        );
                    out.push_back(trunc_op);
                }

                auto cat_op = libflo::operation<narrow_node>::create(
                    d,
                    op->t()->nnode(i)->width(),
                    libflo::opcode::CAT,
                    {trunc, op->t()->nnode(i)}
                    );
                out.push_back(cat_op);
            }
        }

        break;
    }

    case libflo::opcode::ARSH:
    case libflo::opcode::CATD:
    case libflo::opcode::EAT:
    case libflo::opcode::EQ:
    case libflo::opcode::GTE:
    case libflo::opcode::INIT:
    case libflo::opcode::LD:
    case libflo::opcode::LIT:
    case libflo::opcode::LOG2:
    case libflo::opcode::LSH:
    case libflo::opcode::LT:
    case libflo::opcode::MEM:
    case libflo::opcode::MSK:
    case libflo::opcode::MUL:
    case libflo::opcode::NEG:
    case libflo::opcode::NEQ:
    case libflo::opcode::NOP:
    case libflo::opcode::RD:
    case libflo::opcode::RND:
    case libflo::opcode::RST:
    case libflo::opcode::ST:
    case libflo::opcode::WR:
        fprintf(stderr, "Can't narrow operation '%s' in ",
                opcode_to_string(op->op()).c_str());
        op->writeln_debug(stderr);
        abort();
    }

    /* We now need to CATD together a bunch of nodes such that they
     * produce exactly the same result as the wide operation would. */

    /* This contains the previous node in the chain. */
    std::shared_ptr<narrow_node> prev = NULL;

    /* This contains the operation to load from, which is usually the
     * destination, but can be the source for registers. */
    auto cd = op->op() == libflo::opcode::REG ? op->t() : op->d();

    for (size_t i = 0; i < op->d()->nnode_count(); ++i) {
        if (prev == NULL) {
            prev = op->d()->catdnode(i);

            auto ptr = libflo::operation<narrow_node>
                ::create(prev,
                         prev->width_u(),
                         libflo::opcode::MOV,
                         {cd->nnode(0)}
                    );
            out.push_back(ptr);
            continue;
        }

        auto next = op->d()->catdnode(i);
        auto ptr = libflo::operation<narrow_node>::create(next,
                                                          next->width_u(),
                                                          libflo::opcode::CATD,
                                                          {cd->nnode(i),
                                                                  prev}
            );
        out.push_back(ptr);
        prev = next;
    }

    return out;
}
