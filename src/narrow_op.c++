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

/* Bit-Field EXTract: given a wide node this outputs a narrow node
 * that contains the given bit field.  This will output any necessary
 * operations out to the output field.  One of these takes a named
 * output node, the other generates one. */
static
std::shared_ptr<narrow_node> bfext(out_t& out,
                                   size_t width,
                                   const std::shared_ptr<wide_node>& w,
                                   size_t offset,
                                   size_t count);
static void bfext(std::shared_ptr<narrow_node>& n,
                  out_t& out,
                  size_t width,
                  const std::shared_ptr<wide_node>& w,
                  size_t offset,
                  size_t count);

out_t narrow_op(const std::shared_ptr<libflo::operation<wide_node>> op,
                size_t width, bool emit_catd)
{
    /* Check to see if this operation just fits within a machine word,
     * in which case we don't really have to do anything. */
    {
        bool too_large = false;
        for (const auto& o: op->operands())
            if (o->width() > width)
                too_large = true;

        /* If none of the operands were too large to fit within a
         * machine word then just do the cast. */
        if (too_large == false) {
            std::shared_ptr<narrow_node> d;
            std::vector<std::shared_ptr<narrow_node>> s;

            d = narrow_node::clone_from(op->d());
            for (const auto& source: op->sources())
                s.push_back(narrow_node::clone_from(source));

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
    case libflo::opcode::MOV:
    case libflo::opcode::MUX:
    case libflo::opcode::NOT:
    case libflo::opcode::OR:
    case libflo::opcode::OUT:
    case libflo::opcode::REG:
    case libflo::opcode::XOR:
#ifndef MAPPING
    /* If there's no IO mapping needed then we just need to split
     * inputs bitwise, otherwise they'll be split using RSHD nodes. */
    case libflo::opcode::IN:
#endif
    {
        for (size_t i = 0; i < op->d()->nnode_count(); ++i) {
            auto d = op->d()->nnode(i);

            std::vector<std::shared_ptr<narrow_node>> svec;
            for (const auto s: op->sources()) {
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

#ifdef MAPPING
    case libflo::opcode::IN:
    {
        /* IN can be a special case: in order to generate code that
         * matches with the C++ test harness we need to */
        auto wide_in = narrow_node::clone_from(op->d(), true);
        auto wide_op = libflo::operation<narrow_node>::create(
            wide_in,
            wide_in->width_u(),
            libflo::opcode::IN,
            {}
            );
        out.push_back(wide_op);

        /* Now we actually go ahead and do all the CAT nodes. */
        for (size_t i = 0; i < op->d()->nnode_count(); ++i) {
            auto d = op->d()->nnode(i);

            auto offset = i * wide_node::get_word_length();
            auto offsetc = narrow_node::create_const(d, offset);

            auto rshd_op = libflo::operation<narrow_node>::create(
                d,
                d->width_u(),
                libflo::opcode::RSHD,
                {wide_in, offsetc}
                );
            out.push_back(rshd_op);
        }

        break;
    }
#endif

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
                auto carry_here = narrow_node::create_temp(1);
                auto carry_here_op = libflo::operation<narrow_node>::create(
                    carry_here,
                    s->width_u(),
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
                auto is_zero = narrow_node::create_temp(1);
                auto is_zero_op = libflo::operation<narrow_node>::create(
                    is_zero,
                    s->width_u(),
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

        /* Shift operations */
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
                        out.push_back(rsh_op);
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

            if ((lo_bit < t_bit) && (hi_bit <= t_bit)) {
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
                bfext(d,
                      out,
                      width,
                      op->s(),
                      lo_bit - op->t()->width(),
                      hi_bit - lo_bit
                    );
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

        /* Left-shift has pretty much the same limitations as
         * right-shift does. */
    case libflo::opcode::LSH:
        /* We only support constant-offset shifts. */
        if (op->t()->is_const() == false) {
            fprintf(stderr, "Only constant-offset shifts are supported\n");
            abort();
        } else {
            if (op->d()->nnode_count() != 2) {
                fprintf(stderr, "Only two-word left shifts are supported\n");
                abort();
            }

            size_t offset = op->t()->const_int();
            if (offset > width) {
                fprintf(stderr, "Left shifts can't skip a word\n");
                abort();
            }

            auto offset_n = narrow_node::create_const(offset);
            auto width_m_offset = op->d()->nnode(0)->width() - offset;
            auto width_m_offset_n = narrow_node::create_const(width_m_offset);

            ssize_t hi_width = op->d()->nnode(1)->width() - offset;

            /* First handle the low word, it's easy because all we
             * have to do is zero fill it. */
            {
                auto d = op->d()->nnode(0);

                auto lo_op = libflo::operation<narrow_node>::create(
                    d,
                    d->width(),
                    libflo::opcode::LSH,
                    {op->s()->nnode(0), offset_n}
                    );
                out.push_back(lo_op);
            }

            /* Now handle the high word, which is a touch harder
             * because it requires information from two sources to be
             * concatonated together. */
            if ((hi_width > 0) && (op->s()->nnode_count() == 2)) {
                auto lo = narrow_node::create_temp(offset);
                auto lo_op = libflo::operation<narrow_node>::create(
                    lo,
                    lo->width(),
                    libflo::opcode::RSH,
                    {op->s()->nnode(0), width_m_offset_n}
                    );
                out.push_back(lo_op);

                auto hi = narrow_node::create_temp(hi_width);
                auto hi_op = libflo::operation<narrow_node>::create(
                    hi,
                    hi_width,
                    libflo::opcode::RSH,
                    {op->s()->nnode(1), narrow_node::create_const(0)}
                    );
                out.push_back(hi_op);

                auto d = op->d()->nnode(1);
                auto cat_op = libflo::operation<narrow_node>::create(
                    d,
                    d->width(),
                    libflo::opcode::CAT,
                    {hi, lo}
                    );
                out.push_back(cat_op);
            } else if ((hi_width > 0) && (op->s()->nnode_count() == 1)) {
                auto d = op->d()->nnode(1);
                auto lo_op = libflo::operation<narrow_node>::create(
                    d,
                    d->width(),
                    libflo::opcode::RSH,
                    {op->s()->nnode(0), width_m_offset_n}
                    );
                out.push_back(lo_op);
            } else {
                auto d = op->d()->nnode(1);
                auto rsh_w = 0 - hi_width;
                auto mov_op = libflo::operation<narrow_node>::create(
                    d,
                    d->width(),
                    libflo::opcode::RSH,
                    {op->s()->nnode(0), narrow_node::create_const(d, rsh_w)}
                    );
                out.push_back(mov_op);
            }
        }
        break;

        /* Multiplication has more custom splitting rules. */
    case libflo::opcode::MUL:
    {
        if (op->s()->width() != op->t()->width()) {
            fprintf(stderr, "Multiplication with different operand widths\n");
            abort();
        }

        if (op->s()->nnode_count() != 1) {
            fprintf(stderr, "Multiplication with more than one word\n");
            abort();
        }

        auto full_width = op->s()->width();
        auto half_width = op->s()->width() / 2;

        /* We need to split the inputs up into half-width operations,
         * as that's the only way to do a full-precision multiply. */
        auto sl = bfext(out, full_width, op->s(), 0, half_width);
        auto sh = bfext(out, full_width, op->s(), half_width, half_width);
        auto tl = bfext(out, full_width, op->t(), 0, half_width);
        auto th = bfext(out, full_width, op->t(), half_width, half_width);

        /* Now that we've got these half-words, we need to multiply
         * them all together.  These are guarnteed to be narrow
         * operations because they're at most half width. */
        auto mul = [&](std::shared_ptr<narrow_node>& a,
                       std::shared_ptr<narrow_node>& b)
            -> std::shared_ptr<narrow_node>
            {
                auto p = narrow_node::create_temp(full_width);
                auto p_op = libflo::operation<narrow_node>::create(
                    p,
                    full_width,
                    libflo::opcode::MUL,
                    {a, b}
                    );
                out.push_back(p_op);
                return p;
            };
        auto sltl = mul(sl, tl);
        auto shtl = mul(sh, tl);
        auto slth = mul(sl, th);
        auto shth = mul(sh, th);

        /* At this point we're going to start generating some wide
         * operations, which will then get passed back through the
         * multi-word expander. */
        auto wide_ops =
            std::vector<std::shared_ptr<libflo::operation<wide_node>>>();

        /* Expands the given node such that it will fill the full
         * output width.  This pads the MSB with zeros and the LSB
         * with zeros, for the length of that offset. */
        auto expand = [&](std::shared_ptr<narrow_node>& n,
                          size_t offset)
            -> std::shared_ptr<wide_node>
            {
                auto nw = wide_node::clone_from(n);

                auto zero = wide_node::create_const(nw, offset);

                auto nwo = wide_node::create_temp(nw->width() * 2);
                auto nwo_op = libflo::operation<wide_node>::create(
                    nwo,
                    nwo->width(),
                    (offset == 0) ? libflo::opcode::RSH : libflo::opcode::LSH,
                    {nw, zero}
                    );
                wide_ops.push_back(nwo_op);

                return nwo;
            };
        auto sltle = expand(sltl, 0*half_width);
        auto shtle = expand(shtl, 1*half_width);
        auto slthe = expand(slth, 1*half_width);
        auto shshe = expand(shth, 2*half_width);

        /* At this point we just need to sum up every word. */
        auto sum = [&](std::vector<std::shared_ptr<wide_node>> values)
            -> std::shared_ptr<wide_node>
            {
                std::shared_ptr<wide_node> s = NULL;

                for (auto& value: values) {
                    if (s == NULL) {
                        s = value;
                    } else {
                        auto ns = wide_node::create_temp(s);
                        auto ns_op = libflo::operation<wide_node>::create(
                            ns,
                            ns->width(),
                            libflo::opcode::ADD,
                            {s, value}
                            );
                        wide_ops.push_back(ns_op);
                        s = ns;
                    }
                }

                return s;
            };
        auto total = sum({sltle, shtle, slthe, shshe});

        std::shared_ptr<wide_node> d = op->d();
        auto mov_op = libflo::operation<wide_node>::create(
            d,
            d->width(),
            libflo::opcode::MOV,
            {total}
            );
        wide_ops.push_back(mov_op);

        /* Pass every wide op we generated through the multi-word
         * expander to produce more operations! */
        for (const auto& wide_op: wide_ops) {
            for (const auto& op: narrow_op(wide_op, width, false))
                out.push_back(op);
        }

        break;
    }

        /* Negation is really just subtraction... */
    case libflo::opcode::NEG:
    {
        auto zero = wide_node::create_const(op->s()->width(), 0);
        auto d = op->d();
        auto sub_op = libflo::operation<wide_node>::create(
            d,
            d->width(),
            libflo::opcode::SUB,
            {zero, op->s()}
            );

        for (const auto& op: narrow_op(sub_op, width, false))
            out.push_back(op);

        break;
    }

        /* Some operations are fundamentally unsplitable. */
    case libflo::opcode::CATD:
    case libflo::opcode::RSHD:
        fprintf(stderr, "It's impossible to narrow debug operations\n");
        op->writeln_debug(stderr);
        abort();
        break;

        /* Comparison operations need to be narrowed a bit differently
         * than everything else does because they're actually a
         * reduction not a mapping. */
    case libflo::opcode::EQ:
    case libflo::opcode::NEQ:
    {
        auto reduction = narrow_node::create_temp(op->d()->width());

        /* Equality is simple: we need every word to be equal between
         * the two nodes.  Thus we simply do an AND reduction here. */
        {
            auto defval = op->op() == libflo::opcode::EQ ? 1 : 0;
            auto o = narrow_node::create_const(reduction, defval);
            auto mov_op = libflo::operation<narrow_node>::create(
                reduction,
                reduction->width_u(),
                libflo::opcode::MOV,
                {o}
                );
            out.push_back(mov_op);
        }

        /* Now walk through and check every word for equality. */
        for (size_t i = 0; i < op->s()->nnode_count(); ++i) {
            auto s = op->s()->nnode(i);
            auto t = op->t()->nnode(i);

            auto cur = narrow_node::create_temp(reduction);
            auto cur_op = libflo::operation<narrow_node>::create(
                cur,
                s->width_u(),
                op->op(),
                {s, t}
                );
            out.push_back(cur_op);

            auto opcode = op->op() == libflo::opcode::EQ
                ? libflo::opcode::AND : libflo::opcode::OR;
            auto nred = narrow_node::create_temp(reduction);
            auto and_op = libflo::operation<narrow_node>::create(
                nred,
                nred->width_u(),
                opcode,
                {reduction, cur}
                );
            out.push_back(and_op);

            reduction = nred;
        }

        /* Finally go and overwrite the output node. */
        {
            auto d = op->d()->nnode(0);
            auto mov_op = libflo::operation<narrow_node>::create(
                d,
                d->width_u(),
                libflo::opcode::MOV,
                {reduction}
                );
            out.push_back(mov_op);
        }

        break;
    }

    case libflo::opcode::GTE:
    case libflo::opcode::LT:
    {
        auto reduction = narrow_node::create_temp(op->d()->width());

        /* I guess this is some sort of reduction...  Just with a MUX
         * instead of an AND/OR.  There's probably a word for
         * it... :) */
        {
            auto defval = op->op() == libflo::opcode::GTE ? 1 : 0;
            auto o = narrow_node::create_const(reduction, defval);
            auto mov_op = libflo::operation<narrow_node>::create(
                reduction,
                reduction->width_u(),
                libflo::opcode::MOV,
                {o}
                );
            out.push_back(mov_op);
        }

        /* Here's the reduction.  Essentially we compute the value at
         * every word and pass through the old value if the current
         * words are equal. */
        for (size_t i = 0; i < op->s()->nnode_count(); ++i) {
            auto s = op->s()->nnode(i);
            auto t = op->t()->nnode(i);

            auto cur = narrow_node::create_temp(reduction);
            auto cur_op = libflo::operation<narrow_node>::create(
                cur,
                s->width_u(),
                op->op(),
                {s, t}
                );
            out.push_back(cur_op);

            auto pass = narrow_node::create_temp(reduction);
            auto pass_op = libflo::operation<narrow_node>::create(
                pass,
                s->width_u(),
                libflo::opcode::EQ,
                {s, t}
                );
            out.push_back(pass_op);

            auto nred = narrow_node::create_temp(reduction);
            auto mux_op = libflo::operation<narrow_node>::create(
                nred,
                nred->width_u(),
                libflo::opcode::MUX,
                {pass, reduction, cur}
                );
            out.push_back(mux_op);

            reduction = nred;
        }

        /* Finally go and overwrite the output node. */
        {
            auto d = op->d()->nnode(0);
            auto mov_op = libflo::operation<narrow_node>::create(
                d,
                d->width_u(),
                libflo::opcode::MOV,
                {reduction}
                );
            out.push_back(mov_op);
        }

        break;
    }

    case libflo::opcode::LOG2:
    {
        /* The log operation is another sort of reduction with a bunch
         * of MUXes. */
        /* FIXME: This won't work for nodes lager than 2^31... :). */
        auto reduction = narrow_node::create_temp(width);
        {
            auto z = narrow_node::create_const(reduction, -1);
            auto mov_op = libflo::operation<narrow_node>::create(
                reduction,
                reduction->width_u(),
                libflo::opcode::MOV,
                {z}
                );
            out.push_back(mov_op);
        }

        /* Here's the actual reduction code. */
        for (size_t i = 0; i < op->s()->nnode_count(); ++i) {
            auto s = op->s()->nnode(i);

            auto cur = narrow_node::create_temp(reduction);
            auto cur_op = libflo::operation<narrow_node>::create(
                cur,
                cur->width_u(),
                libflo::opcode::LOG2,
                {s}
                );
            out.push_back(cur_op);

            auto offset = narrow_node::create_const(cur, i * width);
            auto sum = narrow_node::create_temp(cur);
            auto sum_op = libflo::operation<narrow_node>::create(
                sum,
                sum->width_u(),
                libflo::opcode::ADD,
                {cur, offset}
                );
            out.push_back(sum_op);

            auto pass = narrow_node::create_temp(1);
            auto zero = narrow_node::create_const(s, 0);
            auto pass_op = libflo::operation<narrow_node>::create(
                pass,
                s->width_u(),
                libflo::opcode::EQ,
                {s, zero}
                );
            out.push_back(pass_op);

            auto nred = narrow_node::create_temp(reduction);
            auto mux_op = libflo::operation<narrow_node>::create(
                nred,
                nred->width_u(),
                libflo::opcode::MUX,
                {pass, reduction, sum}
                );
            out.push_back(mux_op);

            reduction = nred;
        }

        /* Now we actually have to fill in the output node, which
         * could be very wide.  Essentially this just requires a
         * sign-extension of the one word that we've actually
         * calculated as being useful. */
        {
            auto wmo = narrow_node::create_const(width - 1);
            auto sign_bit = narrow_node::create_temp(1);
            auto sign_bit_op = libflo::operation<narrow_node>::create(
                sign_bit,
                sign_bit->width_u(),
                libflo::opcode::RSH,
                {reduction, wmo}
                );
            out.push_back(sign_bit_op);

            auto sign_word = narrow_node::create_temp(width);
            auto zero = narrow_node::create_const(sign_word, 0);
            auto sign_word_op = libflo::operation<narrow_node>::create(
                sign_word,
                sign_word->width_u(),
                libflo::opcode::ARSH,
                {sign_bit, zero}
                );
            out.push_back(sign_word_op);

            for (size_t i = 0; i < op->d()->nnode_count(); ++i) {
                auto d = op->d()->nnode(i);
                auto s = (i == 0) ? reduction : sign_word;

                auto mov_op = libflo::operation<narrow_node>::create(
                    d,
                    d->width_u(),
                    libflo::opcode::RSH,
                    {s, zero}
                    );
                out.push_back(mov_op);
            }
        }

        break;
    }

    case libflo::opcode::ARSH:
    case libflo::opcode::DIV:
    case libflo::opcode::EAT:
    case libflo::opcode::INIT:
    case libflo::opcode::LD:
    case libflo::opcode::LIT:
    case libflo::opcode::MEM:
    case libflo::opcode::MSK:
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

    /* Here's an early out: if there's only one destination node then
     * there's no reason to bother emiting a CATD to put them
     * together. */
    if (op->d()->nnode_count() == 1)
        return out;

    /* IN nodes aren't handled with a CATD at all. */
    if (op->op() == libflo::opcode::IN)
        return out;

    /* If we've been asked not to emit CATD nodes then don't do so. */
    if (emit_catd == false)
        return out;

    /* We now need to CATD together a bunch of nodes such that they
     * produce exactly the same result as the wide operation would.
     * This mapping is not produced for all configurations. */

#ifdef MAPPING
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
#endif

    return out;
}

std::shared_ptr<narrow_node> bfext(out_t& out,
                                   size_t width,
                                   const std::shared_ptr<wide_node>& w,
                                   size_t offset,
                                   size_t count)
{
    auto n = narrow_node::create_temp(count);
    bfext(n, out, width, w, offset, count);
    return n;
}

void bfext(std::shared_ptr<narrow_node>& n,
           out_t& out,
           size_t width,
           const std::shared_ptr<wide_node>& w,
           size_t offset,
           size_t count)
{
    size_t lo_word = offset / width;
    size_t hi_word = (offset + count - 1) / width;

    if (lo_word == hi_word) {
        auto moffset = offset % width;
        auto rsh_op = libflo::operation<narrow_node>::create(
            n,
            n->width_u(),
            libflo::opcode::RSH,
            {w->nnode(lo_word), narrow_node::create_const(moffset)}
            );
        out.push_back(rsh_op);
    } else if (hi_word == (lo_word + 1)) {
        auto lo_offset = offset % width;

        auto lo_width = w->nnode(lo_word)->width() - lo_offset;
        auto lo_dat = narrow_node::create_temp(lo_width);
        auto lo_op = libflo::operation<narrow_node>::create(
            lo_dat,
            lo_dat->width_u(),
            libflo::opcode::RSH,
            {w->nnode(lo_word), narrow_node::create_const(lo_offset)}
            );
        out.push_back(lo_op);

        auto hi_width = n->width() - lo_width;
        auto hi_dat = narrow_node::create_temp(hi_width);
        auto hi_op = libflo::operation<narrow_node>::create(
            hi_dat,
            hi_dat->width_u(),
            libflo::opcode::RSH,
            {w->nnode(hi_word), narrow_node::create_const(0)}
            );
        out.push_back(hi_op);

        auto cat_op = libflo::operation<narrow_node>::create(
            n,
            n->width_u(),
            libflo::opcode::CAT,
            {hi_dat, lo_dat}
            );
        out.push_back(cat_op);
    } else {
        fprintf(stderr, "Non-contiguous words extracted\n");
        fprintf(stderr, "  offset: " SIZET_FORMAT "\n", offset);
        fprintf(stderr, "  count: " SIZET_FORMAT "\n", count);
        fprintf(stderr, "  lo_word: " SIZET_FORMAT "\n", lo_word);
        fprintf(stderr, "  hi_word: " SIZET_FORMAT "\n", hi_word);
        abort();
    }
}
