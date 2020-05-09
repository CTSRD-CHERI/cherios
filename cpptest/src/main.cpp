/*-
 * Copyright (c) 2017 Lawrence Esswood
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract FA8750-10-C-0237
 * ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

extern "C" {
#include "cheric.h"
#include "object.h"
#include "stdio.h"
}

#pragma clang diagnostic ignored "-Wunused-parameter"
#include <vector>

class Base {
public:
    virtual void v_f() = 0;
};

class Foo : public Base {
public:
    virtual void v_f() {
        printf("I am the parent (virtual)!\n");
    }
    void v_s() {
        static int some_int = 0;
        printf("I am the parent (static)! %d\n", some_int++);
    }
};

class Bar : public Foo {
public:
    virtual void v_f() {
        printf(("I am the child (virtual)!\n"));
    }
    void v_s() {
        printf("I am the child (static)!\n");
    }
};

class Another {

};

class Weird : public Another, public Foo {
    virtual void v_f() {
        printf(("I double inherited (virtual)!\n"));
    }
};

__attribute__((noinline)) void call_some_funcs(Base* f) {
    f->v_f();
}

int test() {
    Foo F;
    Bar B;
    Weird W;

    printf("Call both functions on F\n");
    F.v_f();
    F.v_s();

    printf("Call both functions on B\n");
    B.v_f();
    B.v_s();

    Foo* FR = (Foo*)&F;
    Foo* BR = (Foo*)&B;

    printf("Call both functions on Foo ref\n");
    FR->v_f();
    FR->v_s();

    printf("Call both functions on Bar ref\n");
    BR->v_f();
    BR->v_s();

    printf("Via a no-inline function\n");
    call_some_funcs(FR);
    call_some_funcs(BR);
    call_some_funcs(&W);

    printf("Using some LibCXX\n");

    std::vector<int> some_ints;

    some_ints.push_back(1);
    some_ints.push_back(2);
    some_ints.push_back(3);
    some_ints.push_back(4);
    some_ints.push_back(5);

    for(auto i : some_ints) {
        printf("An int: %d\n", i);
    }

    return 0;
}

extern "C" {
int main(void) {
    return test();
}
}
