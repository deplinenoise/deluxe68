# Deluxe68

Deluxe68 is a simple (stupid) register allocator frontend for 68k assembly. It
is a source to source translator, so you'll need your regular assembler to
assemble its output. All it does is automate some tedious register allocation
for you.

## Usage

Usage is simple:

    deluxe68 input.s output.s

## Marking up source code

The following extensions are provided:

### Allocating registers

To pull from the data register pool, use `@dreg`:

                @dreg   a, b, [...]
                moveq   #0,@a
                moveq   #1,@b

Similarly, `@areg` allocates address registers. The stack pointer is
automatically reserved and will never be allocated:

                @areg   ptr
                lea     foo(pc),@ptr

### Killing registers

Use `@kill` to return a register to the pool:

                @dreg   a
                [.. code using @a ..]
                @kill   a
                moveq   #0,@a           ; now generates an error!

### Renaming an allocated register

Often in assembly programming, the purpose of a register changes. You can
express that by renaming the register:

                ; @xcoordptr = x coordinate buffer

                @dreg	x0,x1
                move.w	(@xcoordptr)+,@x0
                move.w	(@xcoordptr)+,@x1
                sub.w	@x0,@x1
                @rename @x1,@deltax         ; @x1 is now no longer a coordinate 

                ; ...

                @kill x0,deltax             ; x1 is now gone, and using it will result in an error

### Using allocated registers

You can subsitute `@name` for a register in any instruction or macro
invokation. The only caveat is if the register has been spilled, in which case
you'll instead get a reference to the stack which can generate a
memory-to-memory instruction that doesn't assemble. In that case, rework the
code.

### Spilling and restoring registers

To explicitly spill a named register to the stack (returning it to the pool) you can use `@spill`:

                @dreg   a
                moveq   #0,@a
                @spill  a               ; a is now on stack
                ...
                ...                     ; more code involving more data register allocation
                ...
                @restore a              ; a is now back in the same register it lived in before

`@spill` and `@restore` can also work with real registers. Spilling a real register ensures that
there is nothing named in that real register. This is useful when calling external code.

### Reserving and unreserving registers

To reserve a real register you can use `@reserve`:

                @reserve d0             ; d0 is no longer available to the allocator
                moveq   #0,d0
                ...
                @unreserve d0           ; return d0 to the register allocator

### Calling subroutines

When you want to call a subroutine then you typically need to place arguments in specific registers.
Use `@spill`/`@reserve` pairs to prepare the registers for use, and `@unreserve`/`@restore` pairs
when you are done:

                @spill  a0,d1
                @reserve a0,d1
                move.l  @foo,a0
                move.l  @bar,d1
                bsr     SomeExternalCode
                @unreserve a0,d1
                @restore a0,d1

If `a0` or `d1` are not allocated, the `@spill`/`@restore` operations will do nothing.
The `@reserve`/`@unreserve` operations are for bookkeeping, and will generate no code.


### Procedures

Mark a procedure entry point with `@proc ProcedureName(<reg>: name, [<reg>: name ...])`. You can
also use `@proc ProcedureName` (that is, omitting the register-name part
entirely) if your procedure has no arguments.

Doing so accomplishes two things:

- It generates an automatic `movem.l` that stores all touched registers to the stack
- All live registers are killed automatically

Similarly, instead of `rts`, use `@endproc`. This puts the inverse `movem.l` in
place, and also emits the `rts` instruction.

Any registers declared in the procedure header are automatically live and not
available for allocation in the procedure. You can however `@kill` them to
return them to the pool.

### Example

This input:

                        @proc   Foo(a0:ptr, d0:count)

                        @dreg   sum

                        moveq   #0,@sum
                        subq    #1,@count
        .loop           add.w   (@ptr)+,@sum
                        dbf     @count,.loop

                        move.w  @sum,d0
                        @endproc

Generates output similar to:

                        ; @proc   Foo(a0:ptr, d0:count)
                        ; live reg a0 => ptr
                        ; live reg d0 => count

        Foo:
                        movem.l d1,-(sp)

                        ; @dreg   sum
                        ; live reg d1 => sum

                        moveq   #0,d1
                        subq    #1,d0
        .loop           add.w   (a0)+,d1
                        dbf     d0,.loop

                        move.w  d1,d0
                        ; @endproc
                        movem.l (sp)+,d1
                        rts

### License

This software is available under the BSD 2-clause license:

Copyright (c) 2016, Andreas Fredriksson
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

