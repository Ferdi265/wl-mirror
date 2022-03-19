#ifndef WL_MIRROR_MIRROR_BACKENDS_H_
#define WL_MIRROR_MIRROR_BACKENDS_H_

struct ctx;

void init_mirror_dmabuf(struct ctx * ctx);
void init_mirror_screencopy(struct ctx * ctx);

#endif
