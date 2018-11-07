#include "vm/page.h"

unsigned page_hash_hash_helper(const struct hash_elem * element, void * aux);
bool page_hash_less_helper(const struct hash_elem *a, const struct hash_elem *b, void *aux);

void page_init(void)
{
	sema_init(&page_sema, 1);

	// sema_down(&page_sema);
	// hash_init(&page_table, page_hash_hash_helper, page_hash_less_helper, NULL);
	// sema_up(&page_sema);
}

void page_table_init(struct hash* h){

	if(h == NULL) return
	sema_down(&page_sema);
  	hash_init(h, page_hash_hash_helper, page_hash_less_helper, NULL);
  	sema_up(&page_sema);
}

// PTE* page_get_pte(uint32_t *upage)
// {
// 	if(upage==NULL) return NULL;
	
// 	sema_down(&page_sema);
// 	uint32_t *kpage = palloc_get_page(PAL_ZERO);

// 	if(kpage == NULL){
// 		sema_up(&page_sema);
// 		return NULL;
// 	}
	
// 	return page_set_pte(upage, kpage);
// }

// PTE* page_set_pte(uint32_t *upage, uint32_t *kpage)
// {
// 	PTE* new_pte = (PTE*)malloc(sizeof(PTE));
// 	new_pte->paddr = kpage;
// 	new_pte->uaddr = upage;
// 	new_pte->dirty = false;
// 	// new_pte->is_swapped_out = false;
// 	new_pte->ut = thread_current(); 
// 	hash_insert(&thread_current()->pt, &new_pte->helem);
// 	sema_up(&page_sema);
// 	return new_pte;
// }

void page_map(uint32_t *upage, uint32_t *kpage, bool writable)
{
	if(upage == NULL || kpage == NULL) return;
	struct thread *t= thread_current();
	
	sema_down(&page_sema);
	
	PTE *new_pte = (PTE*)malloc(sizeof(PTE));
	new_pte->paddr = kpage;
	new_pte->uaddr = upage;
	new_pte->dirty = false;
	new_pte->is_swapped_out = false;
	new_pte->writable = writable;
	new_pte->usertid = t->tid;
	new_pte->load = false;
	hash_insert(&t->pt, &new_pte->helem);
	

	sema_up(&page_sema);
}

void page_remove_pte(uint32_t *upage)
{
	if(upage==NULL) return;

	sema_down(&page_sema);
	PTE *pte = page_pte_lookup(upage);
	hash_delete(&thread_current()->pt, &pte->helem);
	free(pte);
	sema_up(&page_sema);
}

PTE* page_pte_lookup(uint32_t *addr)
{
	PTE pte;
	struct hash_elem *helem;

	pte.uaddr = addr;
	pte.usertid = thread_current()->tid;
	helem = hash_find(&thread_current()->pt, &pte.helem);

	return helem!=NULL ? hash_entry(helem, PTE, helem) : NULL;
}



unsigned page_hash_hash_helper(const struct hash_elem * element, void * aux)
{
	PTE *pte = hash_entry(element, PTE, helem);
	return hash_bytes(&pte->uaddr, sizeof(uint32_t));
}

bool page_hash_less_helper(const struct hash_elem *a, const struct hash_elem *b, void *aux)
{
	uint32_t* a_key = hash_entry(a, PTE, helem) -> uaddr;
	uint32_t* b_key = hash_entry(b, PTE, helem) -> uaddr;
	if(a_key < b_key) return true;
	else return false;
}