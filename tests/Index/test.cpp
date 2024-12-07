#define expr(v) v

#ifdef expr
int x = expr(1);
#endif

#undef expr

#define expr(v) v

#ifdef expr
int y = expr(expr(1));
#endif

#undef expr
