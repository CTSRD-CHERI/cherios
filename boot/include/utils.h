//
// Created by le277 on 09/05/17.
//

#ifndef CHERIOS_UTILS_H
#define CHERIOS_UTILS_H

static capability rederive_perms(capability source, capability auth) {
    size_t source_base = cheri_getbase(source);
    size_t source_size = cheri_getlen(source);
    size_t source_offset = cheri_getoffset(source);

    size_t code_base_adjust = source_base - cheri_getbase(auth);
    capability ret = cheri_setoffset(auth, code_base_adjust);
    ret = cheri_setbounds(ret, source_size);
    ret = cheri_setoffset(ret, source_offset);

    return ret;
}

#endif //CHERIOS_UTILS_H
