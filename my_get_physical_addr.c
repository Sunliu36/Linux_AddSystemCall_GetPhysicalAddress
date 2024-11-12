#include <linux/kernel.h>                
#include <linux/syscalls.h>
#include <linux/mm.h>      //提供 mm_struct, pgd_t, p4d_t, pud_t, pmd_t, pte_t 等頁表相關
#include <linux/highmem.h> //提供 page_to_phys 函數
#include <asm/pgtable.h>   //提供頁表操作的輔助函數 (pgd_offset, p4d_offset, pud_offset, pmd_offset, pte_offset_kernel 等)
#include <linux/sched.h>   // for current task

// 將虛擬地址解析為實體地址的系統呼叫
SYSCALL_DEFINE1(my_get_physical_addresses, unsigned long, virtual_addr) {

		//定義頁表
    pgd_t *pgd;
    p4d_t *p4d;
    pud_t *pud;
    pmd_t *pmd;
    pte_t *pte;
    struct page *page;
    unsigned long pfn;
    unsigned long phys_addr;

    printk(KERN_INFO "Resolving address for vaddr = 0x%lx\n", virtual_addr);

		//逐層解析 Table以取得Physical Address(大至小)
		
    // 取得目前進程的 PGD (Page Global Directory)
    pgd = pgd_offset(current->mm, virtual_addr);
    printk(KERN_INFO "pgd = 0x%px, pgd_val = 0x%lx, pgd_index = %lu\n", pgd, pgd_val(*pgd), pgd_index(virtual_addr));
    if (pgd_none(*pgd) || pgd_bad(*pgd)) {
        printk(KERN_ERR "Invalid PGD entry.\n");
        return 0;
    }

    // 解析 P4D (Page Upper Directory)
    p4d = p4d_offset(pgd, virtual_addr);
    printk(KERN_INFO "p4d = 0x%px, p4d_val = 0x%lx, p4d_index = %lu\n", p4d, p4d_val(*p4d), p4d_index(virtual_addr));
    if (p4d_none(*p4d) || p4d_bad(*p4d)) {
        printk(KERN_ERR "Invalid P4D entry.\n");
        return 0;
    }

    // 解析 PUD (Page Upper Directory)
    pud = pud_offset(p4d, virtual_addr);
    printk(KERN_INFO "pud = 0x%px, pud_val = 0x%lx, pud_index = %lu\n", pud, pud_val(*pud), pud_index(virtual_addr));
    if (pud_none(*pud) || pud_bad(*pud)) {
        printk(KERN_ERR "Invalid PUD entry.\n");
        return 0;
    }

    // 解析 PMD (Page Middle Directory)
    pmd = pmd_offset(pud, virtual_addr);
    printk(KERN_INFO "pmd = 0x%px, pmd_val = 0x%lx, pmd_index = %lu\n", pmd, pmd_val(*pmd), pmd_index(virtual_addr));
    if (pmd_none(*pmd) || pmd_bad(*pmd)) {
        printk(KERN_ERR "Invalid PMD entry.\n");
        return 0;
    }

    // 解析 PTE (Page Table Entry)
    pte = pte_offset_map(pmd, virtual_addr);
    
    printk(KERN_INFO "pte = 0x%px, pte_val = 0x%lx, pte_index = %lu\n", pte, pte_val(*pte), pte_index(virtual_addr));
    if (!pte || pte_none(*pte)) {
        printk(KERN_ERR "PTE not present.\n");
        return 0;
    }

    // 取得頁框號 (Page Frame Number) 並計算實體地址
    pfn = pte_pfn(*pte);
    page = pfn_to_page(pfn);
    printk(KERN_INFO "page_addr = 0x%lx, pfn = 0x%lx\n", (page_to_pfn(page) << PAGE_SHIFT), pfn);

    if (!page) {
        printk(KERN_ERR "Invalid PFN.\n");
        return 0;
    }

    // 計算實體地址
    phys_addr = (pfn << PAGE_SHIFT) | (virtual_addr & ~PAGE_MASK); //(所對應的實體位置＝頁框起始位置+頁內偏移)
    printk(KERN_INFO "Physical address: 0x%lx\n", phys_addr);

    return phys_addr;
}