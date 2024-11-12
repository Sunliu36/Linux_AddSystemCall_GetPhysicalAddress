LinuxOS：Add a system call "my_get_physical_address"
===

## 「Project1 Description」
- Project_Description.md
- [CE6105_Fall2024_LinuxOS_Course Page](https://staff.csie.ncu.edu.tw/hsufh/COURSES/FALL2024/linux_project_1.html)

## 1. How to get the Physical address?

![image](https://hackmd.io/_uploads/HJ8m-E2Z1l.png)


### 根據上圖可以去拆解我們從Linear address轉移至Physical address中的過程：

(1) 在CR3 Register 中包含PGD的起始位置
(2) 使用Global DIR查找PGD
(3) 再使用UPPER DIR查找PUD
(4) 接著使用 MIDDLE DIR 部分查找頁中層目錄
(5) 然後使用 TABLE 部分查找頁表(Page Table)
(6) 最後結合 OFFSET 得到最終的實體位址
:::warning
💡 **說明**
- 上圖是一個四級分頁結構
- 每一級都使用相同索引值找到下一級頁表
- 每個+號表示一次記憶體存取
- 每級頁表都儲存了指向表的實體位址
:::

## 2.Design Program and Compiler

```c=
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
```
### 程式碼說明：
(1) **頁表層級定義**：利用 pgd_t、p4d_t、pud_t、pmd_t 和 pte_t 變數來依序解析分頁層級。
(2) **逐層頁表解析**：從 PGD 開始，逐層找到 P4D、PUD、PMD 和 PTE。如果發現頁表項無效或頁表出錯，則返回 0 表示解析失敗。
(3) **物理地址計算**：從最終頁表項（PTE）中取出有效的物理地址部分，並結合虛擬地址中的偏移量 (OFFSET) 來計算最終物理地址。
## 3.Results

### (1)copy_on_write demo


![image2](https://hackmd.io/_uploads/rJ2rZ4hZkl.png)


#### 說明 :
>fork()被呼叫時，Linux會為當前process創建一個幾乎完全相同的process，複製父行程的記憶體內容，為節省記憶體空間，父、子行程會共享相同的記憶體頁面。而當子行程將 `global_a` 的值從 `123` 修改為 `789` 時，觸發了COW機制，系統會將子行程創建一個新頁面，再把原先父程序的記憶體內容複製到該頁，並把新global_a的logical address映射到新頁面中。

#### 結論 :

>在 `fork()` 之後，子行程和父行程原本共享 `global_a` 的同一個物理地址。但當子行程進行寫入操作（修改 `global_a` 的值）時，因為 COW 機制，子行程獲得了一個新的物理頁面，這導致了 `global_a` 在子行程中的物理地址改變。

![image3](https://hackmd.io/_uploads/S1h8Z42bkx.png)


### (2)Loader demo

#### 1. 沒宣告a[1999999]

![image3-2](https://hackmd.io/_uploads/BkwYMV3byx.png)
![image4](https://hackmd.io/_uploads/HJ6PbNnbkl.png)
>以上輸出結果說明 :  當宣告了陣列a[2000000]時，從physical address的角度來看，可以發現a[0]是有被分配的，但在a[1999999]卻是Null值，代表loader並未在一開始就給予process中所有元素分配physical address，導致後面查詢PMD頁面時，發生錯誤。

#### 2. 有宣告a[1999999]變數後 (補充)

![image5](https://hackmd.io/_uploads/B1ktWN2ZJl.png)
![image6](https://hackmd.io/_uploads/BJ1o-Eh-kx.png)

>以上輸出結果說明 : 經過定義a[1999999]變數之後，loader會將這個明確存取的動作執行，賦予此元素本身physical address的值。

#### 結論：
>在宣告大型陣列時，Linux系統不會馬上將所有data分配實體記憶體，而是會使用延遲分配(lazy allocation)的方式提高memory分配的效率，當其中某個元素被呼叫或使用時，loader才會將其賦值。

## Reference
> https://www.cnblogs.com/muahao/p/10297852.html

>https://blog.csdn.net/qq_36393978/article/details/118157426

>https://linux.laoqinren.net/kernel/memory-page/