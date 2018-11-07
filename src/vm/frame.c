#include "vm/frame.h"

unsigned frame_hash_hash_helper(const struct hash_elem * element, void * aux);
bool frame_hash_less_helper(const struct hash_elem *a, const struct hash_elem *b, void *aux);

void frame_init(void)
{
	sema_init(&frame_sema, 1);

	sema_down(&frame_sema);
	hash_init(&frame_table, frame_hash_hash_helper, frame_hash_less_helper, NULL);
	list_init(&fte_list);
	sema_up(&frame_sema);
}

uint8_t* frame_get_fte(uint32_t *upage, enum palloc_flags flag)
{
	if(upage == NULL) return NULL;

	sema_down(&frame_sema);
	uint32_t *kpage = palloc_get_page(flag);
	if(kpage == NULL)
	{
		sema_up(&frame_sema); 
		return NULL;
	}
	frame_set_fte(upage, kpage);
	sema_up(&frame_sema);
	return kpage;
}

bool frame_set_fte(uint32_t *upage, uint32_t *kpage)
{
	FTE* new_fte = (FTE*)malloc(sizeof(FTE));
	new_fte->paddr = kpage;
	new_fte->uaddr = upage;
	// new_fte->is_swapped_out = false;
	new_fte->usertid = thread_current()->tid;

	hash_insert(&frame_table, &new_fte->helem);
	list_push_back(&fte_list, &new_fte->elem);
	return true;
}

void frame_remove_fte(uint32_t* upage)
{
	if(upage==NULL) return;
	sema_down(&frame_sema);

	FTE* fte = frame_fte_lookup(upage);
	pagedir_clear_page(thread_current()->pagedir, upage);
	palloc_free_page(fte->paddr);
	hash_delete(&frame_table, &fte->helem);
	list_remove(&fte->elem);
	free(fte);

	sema_up(&frame_sema);
}

FTE* frame_fifo_fte(void)
{
	int currtid = thread_current()->tid;
	struct list_elem *e = list_begin(&fte_list);
	for(; e != list_end(&fte_list); e = list_next(e))
	{
		FTE *fte = list_entry(e, FTE, elem);
		if(fte->usertid == currtid)
			return fte;
	}
	printf("No frame for the current thread(%d) to swap out\n", currtid);
	NOT_REACHED();
}

FTE* frame_fte_lookup(uint32_t *addr)
{
	FTE fte;
	struct hash_elem *helem;

	fte.uaddr = addr;
	fte.usertid = thread_current()->tid;
	helem = hash_find(&frame_table, &fte.helem);

	return helem!=NULL ? hash_entry(helem, FTE, helem) : NULL;
}

unsigned frame_hash_hash_helper(const struct hash_elem * element, void * aux)
{
	FTE *fte = hash_entry(element, FTE, helem);
	return hash_bytes(&fte->uaddr, sizeof(uint32_t));
}

bool frame_hash_less_helper(const struct hash_elem *a, const struct hash_elem *b, void *aux)
{
	uint32_t* a_key = hash_entry(a, FTE, helem) -> uaddr;
	uint32_t* b_key = hash_entry(b, FTE, helem) -> uaddr;
	if(a_key < b_key) return true;
	else return false;
}


