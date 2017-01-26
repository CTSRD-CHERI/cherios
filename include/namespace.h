void	namespace_init(capability ns_ref, capability ns_id);
int	namespace_register(int nb, capability ref, capability id);
capability	namespace_get_ref(int nb);
capability	namespace_get_id(int nb);

extern void * namespace_ref;
extern void * namespace_id;
