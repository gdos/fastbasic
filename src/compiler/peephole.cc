/*
 * FastBasic - Fast basic interpreter for the Atari 8-bit computers
 * Copyright (C) 2017,2018 Daniel Serpell
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>
 */

// peephole.cc: Peephole optimizer


// Implements a simple peephole optimizer
class peephole
{
    private:
        bool changed;
        std::vector<codew> &code;
        size_t current;
        // Matching functions for the peephole opt
        bool mtok(size_t idx, enum tokens tok)
        {
            idx += current;
            return idx < code.size() && code[idx].is_tok(tok);
        }
        bool mcbyte(size_t idx, std::string name)
        {
            idx += current;
            return idx < code.size() && code[idx].is_sbyte(name);
        }
        bool mcword(size_t idx, std::string name)
        {
            idx += current;
            return idx < code.size() && code[idx].is_sword(name);
        }
        bool mlabel(size_t idx)
        {
            idx += current;
            return idx < code.size() && code[idx].is_label();
        }
        bool mlblw(size_t idx)
        {
            idx += current;
            return idx < code.size() && code[idx].is_sword() &&
                    code[idx].get_str().find("lbl") != std::string::npos;
        }
        bool mword(size_t idx)
        {
            idx += current;
            return idx < code.size() && code[idx].is_word();
        }
        bool mbyte(size_t idx)
        {
            idx += current;
            return idx < code.size() && code[idx].is_byte();
        }
        std::string lbl(size_t idx)
        {
            idx += current;
            if ( idx < code.size() && code[idx].is_label() )
                return code[idx].get_str();
            else
                return std::string();
        }
        std::string wlbl(size_t idx)
        {
            if ( mlblw(idx) )
                return code[idx+current].get_str();
            else
                return std::string();
        }
        int16_t val(size_t idx)
        {
            idx += current;
            if ( idx < code.size() )
                return code[idx].get_val();
            else
                return 0x8000;
        }
        void del(size_t idx)
        {
            if ( idx+current < code.size() )
            {
                changed = true;
                code.erase( code.begin() + idx + current);
            }
        }
        void ins_w(size_t idx, int16_t x)
        {
            int lnum = 0;
            changed = true;
            if( code.size() > idx + current )
                lnum = code[idx+current].linenum();
            code.insert(code.begin() + idx + current, codew::cword(x, lnum));
        }
        void ins_tok(size_t idx, enum tokens tok)
        {
            int lnum = 0;
            changed = true;
            if( code.size() > idx + current )
                lnum = code[idx+current].linenum();
            code.insert(code.begin() + idx + current, codew::ctok(tok, lnum));
        }
        void copy(size_t idx, size_t from, size_t num)
        {
            changed = true;
            while(num)
            {
                code.insert(code.begin() + idx + current, code[from+current]);
                num--;
                idx++;
                from++;
                if( from >= idx )
                    from++;
            }
        }
        void set_ws(size_t idx, std::string str)
        {
            changed = true;
            code[idx+current] = codew::cword(str, code[idx+current].linenum());
        }
        void set_w(size_t idx, int16_t x)
        {
            changed = true;
            code[idx+current] = codew::cword(x&0xFFFF, code[idx+current].linenum());
        }
        void set_b(size_t idx, int16_t x)
        {
            changed = true;
            code[idx+current] = codew::cbyte(x&0xFF, code[idx+current].linenum());
        }
        void set_tok(size_t idx, enum tokens tok)
        {
            changed = true;
            code[idx+current] = codew::ctok(tok, code[idx+current].linenum());
        }
        // Transforms all "numeric" tokens to TOK_NUM, so that the next phases can
        // optimize
        void expand_numbers()
        {
            for(size_t i=0; i<code.size(); i++)
            {
                current = i;
                // Sequences:
                //   TOK_BYTE / x
                if( mtok(0,TOK_BYTE) && mbyte(1) )
                {
                    set_tok(0, TOK_NUM); set_w(1, val(1)); i++;
                }
                //   TOK_1
                else if( mtok(0,TOK_1) )
                {
                    set_tok(0, TOK_NUM); ins_w(1, 1); i++;
                }
                //   TOK_0
                else if( mtok(0,TOK_0) )
                {
                    set_tok(0, TOK_NUM); ins_w(1, 0); i++;
                }
                //   TOK_NUM / non numeric constant
                else if( mtok(0,TOK_NUM) )
                {
                    if( mcword(1, "AUDF1") )       set_w(1, 0xD200);
                    else if( mcword(1, "AUDCTL") ) set_w(1, 0xD208);
                    else if( mcword(1, "SKCTL") )  set_w(1, 0xD20F);
                    else if( mcword(1, "COLOR0") ) set_w(1, 0x02C4);
                    else if( mcword(1, "PADDL0") ) set_w(1, 0x0270);
                    else if( mcword(1, "STICK0") ) set_w(1, 0x0278);
                    else if( mcword(1, "PTRIG0") ) set_w(1, 0x027C);
                    else if( mcword(1, "STRIG0") ) set_w(1, 0x0284);
                    else if( mcword(1, "CH") )     set_w(1, 0x02FC);
                    else if( mcword(1, "FILDAT") ) set_w(1, 0x02FD);
                }
            }
        }
        // Transforms small "numeric" tokens to TOK_BYTE, TOK_1 and TOK_0
        void shorten_numbers()
        {
            for(size_t i=0; i<code.size(); i++)
            {
                current = i;
                // Sequences:
                //   TOK_NUM / x == 0
                if( mtok(0,TOK_NUM) && mword(1) && val(1) == 0 )
                {
                    del(1); set_tok(0, TOK_0);
                }
                //   TOK_NUM / x == 1
                else if( mtok(0,TOK_NUM) && mword(1) && val(1) == 1 )
                {
                    del(1); set_tok(0, TOK_1);
                }
                //   TOK_NUM / x == -1
                else if( mtok(0,TOK_NUM) && mword(1) && val(1) == -1 )
                {
                    set_tok(0, TOK_1); set_tok(1, TOK_NEG);
                }
                //   TOK_NUM / x < 256
                else if( mtok(0,TOK_NUM) && mword(1) && 0 == (val(1) & ~0xFF) )
                {
                    set_tok(0, TOK_BYTE); set_b(1, val(1));
                }
            }
        }
        // Unused labels removal
        void remove_unused_labels()
        {
            // Go through code accumulating all label expressions
            std::set<std::string> labels;
            for(auto &c: code)
            {
                if( c.is_sword() )
                    labels.insert(c.get_str());
            }
            // And go through code removing labels not in the list
            for(size_t i=0; i<code.size(); i++)
            {
                current = i;
                if( mlabel(0) && !labels.count(lbl(0)) )
                {
                    del(0);
                    i--;
                }
            }
        }
        // Remove extra IOCHN0 tokens
        void trace_iochn()
        {
            // This is the calculated current I/O channel
            int ioch = 0;
            for(current=0; current<code.size(); current++)
            {
                if( mlabel(0) || mtok(0, TOK_CALL) )
                    ioch = 0; // Assume 0 after any label or CALL
                else if( mtok(0, TOK_CLOSE) ||
                         mtok(0, TOK_BPUT) ||
                         mtok(0, TOK_BGET) ||
                         mtok(0, TOK_DRAWTO) ||
                         mtok(0, TOK_GRAPHICS) ||
                         mtok(0, TOK_PLOT) ||
                         mtok(0, TOK_PRINT_EOL) ||
                         mtok(0, TOK_PUT) ||
                         mtok(0, TOK_XIO) )
                {
                    ioch = 0; // Tokens that set IOCHN to 0
                }
                else if( mtok(0,TOK_BYTE) && mcbyte(1, "IOCHN") &&
                        mtok(2,TOK_NUM) && mword(3) && mtok(4,TOK_POKE) )
                {
                    if( ioch == val(3) )
                    {
                        // Remove redundant set IOCHN
                        del(4); del(3); del(2); del(1); del(0);
                        current--;
                    }
                    else
                        ioch = val(3);
                }
                else if( mtok(0,TOK_BYTE) && mcbyte(1, "IOCHN") )
                {
                    ioch = -1;
                }
                else if( mtok(0, TOK_IOCHN0) &&
                         mtok(1,TOK_BYTE) && mcbyte(2, "IOCHN") &&
                         mtok(3,TOK_NUM) && mword(4) && mtok(5,TOK_POKE) )
                {
                    // Setting I/O channel just after IOCHN0, delete redundant one
                    del(0);
                    current++;
                }
                else if( mtok(0, TOK_IOCHN0 ) )
                {
                    if( ioch == 0 )
                    {
                        del(0);
                        current--;
                    }
                    else
                        ioch = 0;
                }
            }
        }

        // Jump threading optimization.
        // Replace JUMP to RET or another JUMP with a direct jump to target.
        void replace_label_targets()
        {
            // Build a MAP of label that go to another target
            std::map<std::string, std::string> tgt;
            for(current=0; current<code.size(); current++)
            {
                if( mlabel(0) )
                {
                    std::string l = lbl(0);
                    size_t i;
                    // Search target - next "normal" instruction
                    for(i=1; i<code.size()-current; i++)
                        if( !mlabel(i) )
                            break;
                        else
                            tgt[l] = lbl(i);
                    if( mtok(i, TOK_RET) )
                        tgt[l] = "__TOK_RET__";
                    else if( mtok(i, TOK_JUMP) && mlblw(i+1) )
                        tgt[l] = wlbl(i+1);
                }
            }
            // Now, replace JUMPS
            if( tgt.size() )
            {
                for(current=0; current<code.size(); current++)
                {
                    if( (mtok(0, TOK_CJUMP) || mtok(0, TOK_JUMP)) && mlblw(1) && tgt.find(wlbl(1)) != tgt.end() )
                    {
                        std::string t = tgt[wlbl(1)];
                        if( t == "__TOK_RET__" )
                        {
                            // We can only replace JUMP to RET with RET, not ConditionalJUMP
                            if( mtok(0, TOK_JUMP) )
                            {
                                set_tok(0, TOK_RET);
                                del(1);
                            }
                            else if( mtok(0, TOK_CJUMP) )
                            {
                                set_tok(0, TOK_CRET);
                                del(1);
                            }
                        }
                        else if (wlbl(1) != t)
                            set_ws(1, t);
                    }
                }
            }
        }
    public:
        peephole(std::vector<codew> &code):
            code(code), current(0)
        {
            expand_numbers();
            do
            {
                changed = false;
                remove_unused_labels();
                replace_label_targets();
                trace_iochn();

                for(size_t i=0; i<code.size(); i++)
                {
                    current = i;
                    // Sequences:
                    //   TOK_NUM / x / TOK_NEG  -> TOK_NUM / -x
                    if( mtok(0,TOK_NUM) && mword(1) && mtok(2,TOK_NEG) )
                    {
                        del(2); set_w(1, - val(1)); i--;
                        continue;
                    }
                    //   TOK_NUM / x / TOK_USHL  -> TOK_NUM / 2*x
                    if( mtok(0,TOK_NUM) && mword(1) && mtok(2,TOK_USHL) )
                    {
                        del(2); set_w(1, 2 * val(1)); i--;
                        continue;
                    }
                    //   TOK_NUM / x / TOK_SHL8  -> TOK_NUM / 256*x
                    if( mtok(0,TOK_NUM) && mword(1) && mtok(2,TOK_SHL8) )
                    {

                        del(2); set_w(1, 256 * val(1)); i--;
                        continue;
                    }
                    //   TOK_NUM / 4 / TOK_MUL   -> TOK_USHL TOK_USHL
                    if( mtok(0,TOK_NUM) && mword(1) && val(1) == 4 && mtok(2,TOK_MUL) )
                    {
                        del(2); set_tok(1, TOK_USHL); set_tok(0, TOK_USHL); i--;
                        continue;
                    }
                    //   TOK_NUM / 2 / TOK_MUL   -> TOK_USHL
                    if( mtok(0,TOK_NUM) && mword(1) && val(1) == 2 && mtok(2,TOK_MUL) )
                    {
                        del(2); del(1); set_tok(0, TOK_USHL); i--;
                        continue;
                    }
                    //   TOK_NUM / 1 / TOK_MUL   -> -
                    if( mtok(0,TOK_NUM) && mword(1) && val(1) == 1 && mtok(2,TOK_MUL) )
                    {
                        del(2); del(1); del(0); i--;
                        continue;
                    }
                    //   TOK_NUM / 1 / TOK_DIV   -> -
                    if( mtok(0,TOK_NUM) && mword(1) && val(1) == 1 && mtok(2,TOK_DIV) )
                    {
                        del(2); del(1); del(0); i--;
                        continue;
                    }
                    //   TOK_NUM / 0 / TOK_ADD   -> -
                    if( mtok(0,TOK_NUM) && mword(1) && val(1) == 0 && mtok(2,TOK_ADD) )
                    {
                        del(2); del(1); del(0); i--;
                        continue;
                    }
                    //   TOK_NUM / 0 / TOK_SUB   -> -
                    if( mtok(0,TOK_NUM) && mword(1) && val(1) == 0 && mtok(2,TOK_SUB) )
                    {
                        del(2); del(1); del(0); i--;
                        continue;
                    }
                    //   TOK_NUM / 0 / TOK_NEQ   -> TOK_COMP_0
                    if( mtok(0,TOK_NUM) && mword(1) && val(1) == 0 && mtok(2,TOK_NEQ) )
                    {
                        del(2); del(1); set_tok(0,TOK_COMP_0); i--;
                        continue;
                    }
                    //   TOK_BYTE / 0 / TOK_EQ   -> TOK_COMP_0 TOK_L_NOT
                    if( mtok(0,TOK_NUM) && mword(1) && val(1) == 0 && mtok(2,TOK_EQ) )
                    {
                        del(2); set_tok(0,TOK_COMP_0); set_tok(1, TOK_L_NOT); i--;
                        continue;
                    }
                    //   TOK_NUM / x / TOK_NUM / y / TOK_ADD   -> TOK_NUM (x+y)
                    if( mtok(0,TOK_NUM) && mword(1) && mtok(2,TOK_NUM) && mword(3) && mtok(4,TOK_ADD) )
                    {
                        set_tok(0, TOK_NUM); set_w(1, val(1)+val(3)); del(4); del(3); del(2); i--;
                        continue;
                    }
                    //   TOK_NUM / x / TOK_NUM / y / TOK_SUB   -> TOK_NUM (x-y)
                    if( mtok(0,TOK_NUM) && mword(1) && mtok(2,TOK_NUM) && mword(3) && mtok(4,TOK_SUB) )
                    {
                        set_tok(0, TOK_NUM); set_w(1, val(1)-val(3)); del(4); del(3); del(2); i--;
                        continue;
                    }
                    //   TOK_ADD / TOK_NUM / x / TOK_ADD
                    //      ->   TOK_NUM x / TOK_ADD / TOK_ADD
                    if( mtok(0,TOK_ADD) && mtok(1,TOK_NUM) && mword(2) && mtok(3,TOK_ADD) )
                    {
                        set_tok(0, TOK_NUM); set_w(1,val(2)); set_tok(2, TOK_ADD); i--;
                        continue;
                    }
                    //   TOK_ADD / TOK_NUM / x / TOK_SUB
                    //      ->   TOK_NUM x / TOK_SUB / TOK_ADD
                    if( mtok(0,TOK_ADD) && mtok(1,TOK_NUM) && mword(2) && mtok(3,TOK_SUB) )
                    {
                        set_tok(0, TOK_NUM); set_w(1,val(2)); set_tok(2, TOK_SUB);
                        set_tok(3, TOK_ADD); i--;
                        continue;
                    }
                    //   TOK_SUB / TOK_NUM / x / TOK_ADD
                    //      ->   TOK_NUM x / TOK_SUB / TOK_SUB
                    if( mtok(0,TOK_SUB) && mtok(1,TOK_NUM) && mword(2) && mtok(3,TOK_ADD) )
                    {
                        set_tok(0, TOK_NUM); set_w(1,val(2)); set_tok(2, TOK_SUB);
                        set_tok(3, TOK_SUB); i--;
                        continue;
                    }
                    //   TOK_SUB / TOK_NUM / x / TOK_SUB
                    //      ->   TOK_NUM x / TOK_ADD / TOK_SUB
                    if( mtok(0,TOK_SUB) && mtok(1,TOK_NUM) && mword(2) && mtok(3,TOK_SUB) )
                    {
                        set_tok(0, TOK_NUM); set_w(1,val(2)); set_tok(2, TOK_ADD);
                        set_tok(3, TOK_SUB); i--;
                        continue;
                    }
                    //   TOK_NUM / x / TOK_NUM / y / TOK_MUL   -> TOK_NUM (x*y)
                    if( mtok(0,TOK_NUM) && mword(1) && mtok(2,TOK_NUM) && mword(3) && mtok(4,TOK_MUL) )
                    {
                        set_tok(0, TOK_NUM); set_w(1, val(1) * val(3)); del(4); del(3); del(2); i--;
                        continue;
                    }
                    //   TOK_NUM / x / TOK_NUM / y / TOK_DIV   -> TOK_NUM (x/y)
                    if( mtok(0,TOK_NUM) && mword(1) && mtok(2,TOK_NUM) && mword(3) && mtok(4,TOK_DIV) )
                    {
                        int16_t div = val(3);
                        if( div )
                            div = val(1) / div;
                        else if( val(1) < 0 )
                            div = 1;  // Probably a bug in the division routine, but we emulate the result
                        else
                            div = -1;
                        set_tok(0, TOK_NUM); set_w(1, div); del(4); del(3); del(2); i--;
 continue;
                    }
                    //   TOK_NUM / x / TOK_NUM / y / TOK_MOD   -> TOK_NUM (x%y)
                    if( mtok(0,TOK_NUM) && mword(1) && mtok(2,TOK_NUM) && mword(3) && mtok(4,TOK_MOD) )
                    {
                        int16_t div = val(3);
                        if( div )
                            div = val(1) % div;
                        else
                            div = val(1);  // Probably a bug in the division routine, but we emulate the result
                        set_tok(0, TOK_NUM); set_w(1, div); del(4); del(3); del(2); i--;
 continue;
                    }
                    //   TOK_NUM / x / TOK_NUM / y / TOK_BIT_AND   -> TOK_NUM (x&y)
                    if( mtok(0,TOK_NUM) && mword(1) && mtok(2,TOK_NUM) && mword(3) && mtok(4,TOK_BIT_AND) )
                    {
                        set_tok(0, TOK_NUM); set_w(1, val(1) & val(3)); del(4); del(3); del(2); i--;
                        continue;
                    }
                    //   TOK_NUM / x / TOK_NUM / y / TOK_BIT_OR   -> TOK_NUM (x|y)
                    if( mtok(0,TOK_NUM) && mword(1) && mtok(2,TOK_NUM) && mword(3) && mtok(4,TOK_BIT_OR) )
                    {
                        set_tok(0, TOK_NUM); set_w(1, val(1) | val(3)); del(4); del(3); del(2); i--;
                        continue;
                    }
                    //   TOK_NUM / x / TOK_NUM / y / TOK_BIT_EXOR   -> TOK_NUM (x^y)
                    if( mtok(0,TOK_NUM) && mword(1) && mtok(2,TOK_NUM) && mword(3) && mtok(4,TOK_BIT_EXOR) )
                    {
                        set_tok(0, TOK_NUM); set_w(1, val(1) ^ val(3)); del(4); del(3); del(2); i--;
                        continue;
                    }
                    //  VAR + VAR    ==>   2 * VAR
                    //   TOK_VAR / x / TOK_VAR / x / TOK_ADD   -> TOK_VAR / x / TOK_USHL
                    if( mtok(0,TOK_VAR_LOAD) && mbyte(1) && mtok(2,TOK_VAR_LOAD) && mbyte(3) && mtok(4,TOK_ADD) && val(1) == val(3) )
                    {
                        set_tok(2, TOK_USHL); del(4); del(3); i--;
                        continue;
                    }
                    //  VAR = VAR + 1   ==>  INC VAR
                    //   TOK_VAR_A / x / TOK_VAR / x / TOK_NUM / 1 / TOK_ADD / TOK_DPOKE
                    //        -> TOK_VAR_A / x / TOK_INC
                    if( mtok(0,TOK_VAR_ADDR) && mbyte(1) &&
                        mtok(2,TOK_VAR_LOAD) && mbyte(3) &&
                        mtok(4,TOK_NUM) && mword(5) && val(5) == 1 &&
                        mtok(6,TOK_ADD) && mtok(7,TOK_DPOKE) &&
                        val(1) == val(3) )
                    {
                        set_tok(2, TOK_INC); del(7); del(6); del(5); del(4); del(3); i--;
                        continue;
                    }
                    //  VAR = VAR - 1   ==>  DEC VAR
                    //   TOK_VAR_A / x / TOK_VAR / x / TOK_NUM / 1 / TOK_SUB / TOK_DPOKE
                    //        -> TOK_VAR_A / x / TOK_DEC
                    if( mtok(0,TOK_VAR_ADDR) && mbyte(1) &&
                        mtok(2,TOK_VAR_LOAD) && mbyte(3) &&
                        mtok(4,TOK_NUM) && mword(5) && val(5) == 1 &&
                        mtok(6,TOK_SUB) && mtok(7,TOK_DPOKE) &&
                        val(1) == val(3) )
                    {
                        set_tok(2, TOK_DEC); del(7); del(6); del(5); del(4); del(3); i--;
                        continue;
                    }
                    //   TOK_BYTE / IOCHN / TOK_NUM / 0 / TOK_POKE  -> TOK_IOCHN0
                    if( mtok(0,TOK_BYTE) && mcbyte(1, "IOCHN") &&
                        mtok(2,TOK_NUM) && mword(3) && val(3) == 0 && mtok(4,TOK_POKE) )
                    {
                        set_tok(0, TOK_IOCHN0); del(4); del(3); del(2); del(1); i--;
                        continue;
                    }
                    // NOT NOT A -> A
                    //   TOK_L_NOT / TOK_L_NOT -> TOK_COMP_0
                    if( mtok(0, TOK_L_NOT) && mtok(1, TOK_L_NOT) )
                    {
                        set_tok(0, TOK_COMP_0); del(1);
                        continue;
                    }
                    // NOT A=B -> A<>B
                    //   TOK_EQ / TOK_L_NOT -> TOK_NEQ
                    if( mtok(0, TOK_EQ) && mtok(1, TOK_L_NOT) )
                    {
                        set_tok(0, TOK_NEQ); del(1);
                        continue;
                    }
                    // NOT A<>B -> A=B
                    //   TOK_NEQ / TOK_L_NOT -> TOK_EQ
                    if( mtok(0, TOK_NEQ) && mtok(1, TOK_L_NOT) )
                    {
                        set_tok(0, TOK_EQ); del(1);
                        continue;
                    }
                    // (bool) != 0  -> (bool)
                    //   TOK_L_AND | TOK_L_OR | TOK_L_NOT |
                    //   TOK_NEQ | TOK_COMP_0 | TOK_EQ |
                    //   TOK_LT | TOK_GT
                    //       / TOK_COMP_0   ->  remove TOK_COMP_0
                    if( (mtok(0, TOK_NEQ) ||
                         mtok(0, TOK_L_AND) ||
                         mtok(0, TOK_L_OR) ||
                         mtok(0, TOK_L_NOT) ||
                         mtok(0, TOK_COMP_0) ||
                         mtok(0, TOK_EQ) ||
                         mtok(0, TOK_LT) ||
                         mtok(0, TOK_GT)) && mtok(1, TOK_COMP_0) )
                    {
                        del(1);
                        continue;
                    }
                    // NOT (A > x)  ==  A <= x  ->  A < x+1
                    //    TOK_NUM / x / TOK_GT / TOK_L_NOT -> TOK_NUM / x+1 / TOK_LT
                    if( mtok(0, TOK_NUM) && mword(1) &&
                        mtok(2, TOK_GT) && mtok(3, TOK_L_NOT) )
                    {
                        if( val(1) == 32637 )
                        {
                            // If x=32767, the expression is always true.
                            // We can't currently delete an expression, so
                            // simply replace it with "OR 1"
                            set_tok(0, TOK_NUM);
                            set_w(1, 1);
                            set_tok(2, TOK_BIT_OR);
                            set_tok(3, TOK_COMP_0);
                        }
                        else
                        {
                            set_tok(0, TOK_NUM);
                            set_w(1, val(1) + 1);
                            set_tok(2, TOK_LT);
                            del(3);
                        }
                    }
                    // STRING[i,n>255] -> STRING[i,255]
                    //   TOK_NUM / > 255 / TOK_STR_IDX
                    //        -> TOK_NUM / 255 / TOK_STR_IDX
                    if( mtok(0, TOK_NUM) && mword(1) && mtok(2, TOK_STR_IDX) &&
                        val(1) > 255 )
                    {
                        set_w(1, 255);
                        continue;
                    }
                    // STRING[i][j,n] -> STRING[i+j-1,n]
                    //   TOK_NUM / 255 / TOK_STR_IDX /
                    //   ( TOK_NUM / j ) || ( TOK_VAR_LOAD / k ) /
                    //   ( TOK_NUM / n ) || ( TOK_VAR_LOAD / n ) /
                    //   TOK_STR_IDX
                    //      ->
                    //   TOK_NUM / j / TOK_ADD / TOK_NUM / 1 / TOK_SUB / TOK_NUM / n / TOK_STR_IDX
                    if( mtok(0, TOK_NUM) && mword(1) && (val(1) == 255) &&
                        mtok(2, TOK_STR_IDX) &&
                        ( mtok(3, TOK_NUM) || mtok(3, TOK_VAR_LOAD) ) &&
                        ( mtok(5, TOK_NUM) || mtok(5, TOK_VAR_LOAD) ) &&
                        mtok(7, TOK_STR_IDX) )
                    {
                        ins_tok(5, TOK_ADD);
                        ins_tok(6, TOK_NUM); ins_w(7, 1);
                        ins_tok(8, TOK_SUB);
                        del(2); del(1); del(0);
                        continue;
                    }
                    // STRING[i,n][j] -> STRING[i+j-1,n-j+1]
                    //   ( TOK_NUM / n ) || ( TOK_VAR_LOAD / n ) / TOK_STR_IDX
                    //   ( TOK_NUM / j ) || ( TOK_VAR_LOAD / k ) /
                    //   TOK_NUM / 255 / TOK_STR_IDX
                    //      ->
                    //   TOK_NUM / j / TOK_ADD / TOK_NUM / 1 / TOK_SUB / TOK_NUM / n /
                    //   TOK_NUM / j / TOK_SUB / TOK_NUM / 1 / TOK_ADD / TOK_STR_IDX
                    if( ( mtok(0, TOK_NUM) || mtok(0, TOK_VAR_LOAD) ) &&
                        mtok(2, TOK_STR_IDX) &&
                        ( mtok(3, TOK_NUM) || mtok(3, TOK_VAR_LOAD) ) &&
                        mtok(5, TOK_NUM) && mword(6) && (val(6) == 255) &&
                        mtok(7, TOK_STR_IDX) )
                    {
                        del(6); del(5);
                        copy(0, 3, 2);
                        ins_tok(2, TOK_ADD);
                        ins_tok(3, TOK_NUM); ins_w(4, 1);
                        ins_tok(5, TOK_SUB);
                        del(8);
                        ins_tok(10, TOK_SUB);
                        ins_tok(11, TOK_NUM); ins_w(12, 1);
                        ins_tok(13, TOK_ADD);
                        continue;
                    }
                    // ASC( STRING[i,X (>=1) ] ) -> PEEK( ADR(STRING) + i )
                    //
                    //   TOK_NUM / X (>=1) / TOK_STR_IDX /
                    //   TOK_NUM / Y (<=X) / TOK_ADD / TOK_PEEK
                    //      ->
                    //   TOK_ADD / TOK_NUM / Y - 1 / TOK_ADD / TOK_PEEK
                    if( mtok(0, TOK_NUM) && mword(1) && (val(1)>0) && mtok(2, TOK_STR_IDX) &&
                        mtok(3, TOK_NUM) && mword(4) && (val(4)<=val(1)) &&
                        (val(4)>=1) && mtok(5, TOK_ADD) && mtok(6, TOK_PEEK) )
                    {
                        set_tok(2, TOK_ADD);
                        set_w(4, val(4) - 1);
                        del(1); del(0);
                        continue;
                    }
                    // NOT (A < x)  ==  A >= x  ->  A > x-1
                    //    TOK_NUM / x / TOK_LT / TOK_L_NOT -> TOK_NUM / x+1 / TOK_LT
                    if( mtok(0, TOK_NUM) && mword(1) &&
                        mtok(2, TOK_LT) && mtok(3, TOK_L_NOT) )
                    {
                        if( val(1) == -32768 )
                        {
                            // If x=-32768, the expression is always true.
                            // We can't currently delete an expression, so
                            // simply replace it with "OR 1"
                            set_tok(0, TOK_NUM);
                            set_w(1, 1);
                            set_tok(2, TOK_BIT_OR);
                            set_tok(3, TOK_COMP_0);
                        }
                        else
                        {
                            set_tok(0, TOK_NUM);
                            set_w(1, val(1) - 1);
                            set_tok(2, TOK_GT);
                            del(3);
                        }
                    }
                    // CALL xxxxx / RETURN  ->  JUMP xxxxx
                    //   TOK_CALL / x / TOK_RET -> TOK_JUMP / x
                    if( mtok(0,TOK_CALL) && mtok(2,TOK_RET) )
                    {
                        set_tok(0, TOK_JUMP); del(2);
                        continue;
                    }
                    // Bypass CJUMP over another JUMP
                    //   TOK_CJUMP / x / TOK_JUMP / y / LABEL x
                    //     -> TOK_L_NOT / TOK_CJUMP / y / LABEL x
                    if( mtok(0,TOK_CJUMP) && mtok(2,TOK_JUMP) && mlabel(4) &&
                        lbl(4) == wlbl(1) )
                    {
                        set_tok(0, TOK_L_NOT); set_tok(2, TOK_CJUMP); del(1);
                        continue;
                    }
                    // Bypass CJUMP over a RET
                    //   TOK_CJUMP / x / TOK_RET / LABEL x
                    //     -> TOK_L_NOT / TOK_CRET / LABEL x
                    if( mtok(0,TOK_CJUMP) && mtok(2,TOK_RET) && mlabel(3) &&
                        lbl(3) == wlbl(1) )
                    {
                        set_tok(0, TOK_L_NOT); set_tok(2, TOK_CRET); del(1);
                        continue;
                    }
                    // Remove dead code after a JUMP
                    if( mtok(0,TOK_JUMP) && !mlabel(2) )
                    {
                        del(2);
                        continue;
                    }
                    // Remove dead code after a RET or END
                    if( (mtok(0, TOK_RET) || mtok(0, TOK_END)) && !mlabel(1) )
                    {
                        del(1);
                        continue;
                    }
                }
            } while(changed);
            shorten_numbers();
        }
};

