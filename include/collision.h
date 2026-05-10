#ifndef COLLISION_H
#define COLLISION_H

#include <EFM8LB1.h>

void collision_init(void);
void collision_task(void);
bit collision_blocked(void);
int collision_get_distance_mm(void);

#endif
