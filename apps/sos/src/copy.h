#pragma once
int64_t copy_sos2vspace(void* src, vaddr_t dest_vaddr,
						struct page_directory* vspace, int64_t len);