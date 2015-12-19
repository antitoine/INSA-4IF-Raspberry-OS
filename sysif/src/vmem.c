#include "vmem.h"
#include "kheap.h"
#include "asm_tools.h"
#include "hw.h"

// Variables globales 
//  Pointeur sur la premiere case de la 
//  table d'occupation des frames
uint8_t * free_frames_table;

void init_page_section(uint32_t * table_iterator, 
                       int page_count, 
                       uint32_t lvl_1_flags, 
                       uint32_t lvl_2_flags, 
                       uint32_t phy_start) 
{
    int lvl_1_page; // Itération sur la table 1 pour allocation table 2
    for ( lvl_1_page = 0 ; lvl_1_page < page_count ; ++lvl_1_page ) 
    {
        // Allocation de la section contenant les adresses des pages des sections de RAM
        uint32_t * second_table_it = (uint32_t*)kAlloc_aligned( SECON_LVL_TT_SIZE, SECON_LVL_TT_INDEX_SIZE ); 
        // Inscription dans la table de premier niveau de l'adresse de la page de second niveau tout juste allouée 
        (*table_iterator) = (uint32_t)second_table_it | lvl_1_flags;
        // On itère sur les pages de second niveau pour renseigner les adresses physiques
        int lvl_2_page;
        for ( lvl_2_page = 0 ; lvl_2_page < SECON_LVL_TT_COUN ; ++lvl_2_page )
        {
            // Inscription de l'adresse physique dans l'entrée de la table de niveau 2
            (*second_table_it) = (phy_start + (lvl_1_page * SECON_LVL_TT_COUN * FRAME_SIZE + 
                                 lvl_2_page * FRAME_SIZE)) | lvl_2_flags;
            // Passage à l'entrée suivante dans la table de niveau 2
            second_table_it++;
        }
        // Passage à l'entrée suivante dans la table de premier niveau
        table_iterator++;
    }
}

void init_occupation_table(void)
{
    free_frames_table = (uint8_t*)kAlloc(FREE_FRAMES_TABLE_SIZE);

    int fm_index;
    for (fm_index = 0; fm_index < FREE_FRAMES_TABLE_SIZE; ++fm_index)
    {
        // Si fm_index en zone occupée 
        // code / données / structures noyaux
        int device_lower_bound = (DEVICE_START/FRAME_SIZE);
        int kernel_upper_bound = ((uint32_t)(kernel_heap_limit)+1)/(FRAME_SIZE);
        if( (fm_index >= 0 && 
             fm_index < kernel_upper_bound) ||
            (fm_index > device_lower_bound && 
             fm_index < FREE_FRAMES_TABLE_SIZE) )
        {
            LOCK_FRAME(free_frames_table[fm_index]);
        } 
        // Sinon zone libre
        else
        {
            FREE_FRAME(free_frames_table[fm_index]);
        }
    }
}

uint32_t init_kern_translation_table(void) 
{
    // Initialisation des variables de flags
    uint32_t device_flags = DEVICE_FLAGS;
    uint32_t table_1_page_flags = TABLE_1_PAGE_FLAGS;
    uint32_t table_2_page_flags = TABLE_2_PAGE_FLAGS;

    // Allocation de la section contenant les adresses des pages des tables de second niveau
    uint32_t * translation_base = (uint32_t*)kAlloc_aligned( FIRST_LVL_TT_SIZE, FIRST_LVL_TT_INDEX_SIZE );
    // Itérateur sur la zone de mémoire allouée pour la premiere page
    uint32_t * first_table_it = translation_base;


    // Pour les pages du kernel -------------------------------------------
    // Calcul du nombre de page 1 necessaires pour mapper de 0x000000000 à kernel_heap_limit
    // On ajoute 1 à kernel_heap_limit car on map depuis l'adresse 0
    int kern_page_count = ((uint32_t)(kernel_heap_limit)+1) / (FRAME_SIZE*SECON_LVL_TT_COUN);

    // On remplit l'espace memoire de la page 1 avec les entrées des pages 2
    init_page_section(first_table_it,
                      kern_page_count,
                      table_1_page_flags, 
                      table_2_page_flags,
                      0x0);

    // Incrément de l'itérateur sur la table 1 pour aller pointer l'équivalent de l'adresse 0x20000000 mappée
    first_table_it = translation_base + (DEVICE_START / (FRAME_SIZE*SECON_LVL_TT_COUN));

    // Pour les pages des devices -------------------------------------------
    // Calcul du nombre de page 1 necessaires pour mapper de 0x20000000 à 0x20FFFFFF
    // On ajoute 1 à la différence d'adresses car on map depuis l'adresse 0x20000000 incluse
    int device_page_count = ((DEVICE_END - DEVICE_START) + 1) /(FRAME_SIZE*SECON_LVL_TT_COUN);

    // On remplit l'espace memoire de la page 1 avec les entrées des pages 2
    // Itération sur la table 1 pour allocation table 2
    init_page_section(first_table_it,
                      device_page_count,
                      table_1_page_flags, 
                      device_flags,
                      DEVICE_START);

    // On retourne l'adresse de la page de niveau 1
    return (uint32_t)translation_base;
}

uint32_t init_ps_translation_table(void)
{
    // Initialisation des variables de flags
    uint32_t device_flags = DEVICE_FLAGS;
    uint32_t table_1_page_flags = TABLE_1_PAGE_FLAGS;
    uint32_t table_2_page_flags = TABLE_2_PAGE_FLAGS;

    // Allocation de la section contenant les adresses des pages des tables de second niveau
    uint32_t * translation_base = (uint32_t*)kAlloc_aligned( FIRST_LVL_TT_SIZE, FIRST_LVL_TT_INDEX_SIZE );
    // Itérateur sur la zone de mémoire allouée pour la premiere page
    uint32_t * first_table_it = translation_base;


    // Pour les pages du kernel -------------------------------------------
    // Calcul du nombre de page 1 necessaires pour mapper de 0x000000000 à kernel_heap_limit
    // On ajoute 1 à kernel_heap_limit car on map depuis l'adresse 0
    int kern_page_count = ((uint32_t)(kernel_heap_limit)+1) / (FRAME_SIZE*SECON_LVL_TT_COUN);

    // On remplit l'espace memoire de la page 1 avec les entrées des pages 2
    init_page_section(first_table_it,
                      kern_page_count,
                      table_1_page_flags, 
                      table_2_page_flags,
                      0x0);

    // Incrément de l'itérateur sur la table 1 pour aller pointer l'équivalent de l'adresse 0x20000000 mappée
    first_table_it = translation_base + (DEVICE_START / (FRAME_SIZE*SECON_LVL_TT_COUN));

    // Pour les pages des devices -------------------------------------------
    // Calcul du nombre de page 1 necessaires pour mapper de 0x20000000 à 0x20FFFFFF
    // On ajoute 1 à la différence d'adresses car on map depuis l'adresse 0x20000000 incluse
    int device_page_count = ((DEVICE_END - DEVICE_START) + 1) /(FRAME_SIZE*SECON_LVL_TT_COUN);

    // On remplit l'espace memoire de la page 1 avec les entrées des pages 2
    // Itération sur la table 1 pour allocation table 2
    init_page_section(first_table_it,
                      device_page_count,
                      table_1_page_flags, 
                      device_flags,
                      DEVICE_START);

    // On retourne l'adresse de la page de niveau 1
    return (uint32_t)translation_base;
}

void vmem_init(void) 
{
    // Initialisation de la table d'occupation des frames
    init_occupation_table();
    // Initialisation de la mémoire physique
    kernel_page_table_base = init_kern_translation_table();
    // Configuration de la MMU
    configure_mmu_C(kernel_page_table_base);
    // Activation des interruptions et data aborts
    ENABLE_AB();
    // Démarrage de la MMU
    start_mmu_C();
}

uint8_t * vmem_alloc_in_userland(pcb_s * process, uint32_t size)
{
    uint32_t * device_limit = process->page_table + (DEVICE_START / (FRAME_SIZE*SECON_LVL_TT_COUN));

    // Trouver une page libre --------------------------------
    // Iterateur sur la premiere entree de niveau 1
    uint32_t * table_1_it = process->page_table;
    int lvl_1_ind, lvl_2_ind;
    int ram_overflow_flag = 0;
    int found_flag = 0;
    // Pour chaque entrée de niveau 1
    for (lvl_1_ind = 0; 
        !found_flag && lvl_1_ind < FIRST_LVL_TT_COUN; 
        ++lvl_1_ind, ++table_1_it)
    {   // Si l'entrée de niveau 1 est libre on sort avec lvl_2_ind à 0
        if( ! ((*table_1_it) & 0x03) )
        {
            found_flag = 1;
            lvl_2_ind = 0;
            break;
        }
        else if( table_1_it > device_limit )
        {
            ram_overflow_flag = 1;
            break;
        }
        // Sinon alors l'entrée pointe sur un niveau 2 partiellement ou completement plein 
        else
        {   uint32_t * table_2_it = (uint32_t*)((*table_1_it) & 0xFFFFFC00);
            // Pour chque entrée de niveau 2
            for (lvl_2_ind = 0; 
                lvl_2_ind < SECON_LVL_TT_COUN; 
                ++lvl_2_ind, ++table_2_it)
            {
                if( ! ((*table_2_it) & 0x03) )
                {
                    found_flag = 1;
                    break;
                }
            }
        }
    }
    // Si le ram overflow flag est levé
    if (ram_overflow_flag)
    {   
        return 0x0;
    }

    // Allouer frames -----------------------------------------
    // On calcule le nombre de frames à allouer
    int nb_frame_to_alloc = (size/FRAME_SIZE)+1;
    uint32_t table_2_page_flags = TABLE_2_PAGE_FLAGS;

    // Première entrée
    uint32_t * first_entry_adr_init = process->page_table + lvl_1_ind;

    // Si la première entrée n'est pas initialisée (deux derniers bits à 0), on le fait en appelant init_page_section
    if (!((*first_entry_adr_init) & 0x03)) {
        init_page_section(first_entry_adr_init,
                      1,
                      TABLE_1_PAGE_FLAGS, 
                      TABLE_2_PAGE_FLAGS,
                      (uint32_t) (kernel_heap_limit+1));
    }

   /* uint32_t * first_entry = (uint32_t*)(
            *(first_entry_adr_init) & 0xFFFFFC00) + 
            lvl_2_ind;*/

    // Pour chaque frame à allouer
    int fta; // frame to allocate index
    int ind_2, ind_1;

    uint32_t firstPhysicalAddress = 0;

    for (fta=0; fta < nb_frame_to_alloc; ++fta)
    {
        ind_2 = (lvl_2_ind + fta) % SECON_LVL_TT_COUN;
        ind_1 = lvl_1_ind + ((lvl_2_ind+fta)/SECON_LVL_TT_COUN);
        // Index de la premiere frame libre
        int ff_ind = find_next_free_frame();
        if(ff_ind == -1)
        {
            return 0x0;
        }
        // Adresse physique de la frame libre
        uint32_t f_phy_addr = ff_ind * FRAME_SIZE;

        if (firstPhysicalAddress == 0) {
            firstPhysicalAddress = f_phy_addr;
        }

        // On va chercher l'entrée de niveau 2
        uint32_t * new_entry = (uint32_t*)(
            *(process->page_table + ind_1) & 0xFFFFFC00);
        // On affecte l'entrée de niveau 2 décalée de l'index
        *(new_entry+ind_2) = f_phy_addr | table_2_page_flags;
        // On spécifie que la frame est allouée
        LOCK_FRAME(free_frames_table[ff_ind]);
    }

    // Adresse virtuelle
    uint32_t modifiedVirtualAddress = 0x0;

    // First-level table index dans l'adresse virtuelle
    modifiedVirtualAddress |= (lvl_1_ind << 20);

    // Second-level table index
    modifiedVirtualAddress |= (lvl_2_ind << 12);

    // Page index
    uint32_t pageIndex = firstPhysicalAddress & 0xC;
    modifiedVirtualAddress |= pageIndex;

    return (uint8_t*)modifiedVirtualAddress;
}

int find_next_free_frame(void) 
{
    int f;
    for(f=0; f < FREE_FRAMES_TABLE_SIZE; ++f)
    {   if(IS_FREE_FRAME(free_frames_table[f]))
        {   return f;   
        } 
    }
    return -1;
}

void vmem_free(uint8_t* vAddress, pcb_s * process, uint32_t size)
{
    // Valeur des indexes
    uint32_t va = (uint32_t) vAddress;
    uint32_t first_level_index = (va >> 20);
    uint32_t second_level_index = ((va << 12) >> 24);

    // Nombre de frames à libérer
    int nb_frame_to_dealloc = (size/FRAME_SIZE)+1;

    // Libération des frames ----------------------------------
    int fta; // frame to allocate index
    int ind_2, ind_1;
    for (fta=0; fta < nb_frame_to_dealloc; ++fta)
    {
        ind_2 = (second_level_index + fta) % SECON_LVL_TT_COUN;
        ind_1 = first_level_index + ((second_level_index+fta)/SECON_LVL_TT_COUN);
        // On va chercher l'entrée de niveau 2
        uint32_t * del_entry = (uint32_t*)(
            *(process->page_table + ind_1) & 0xFFFFFC00);
        // On affecte l'entrée de niveau 2 décalée de l'index
        uint32_t del_addr = *(del_entry+ind_2) & 0xFFFFF000;
        int del_frame_ind = del_addr/FRAME_SIZE; 
        // On spécifie que la frame est libérée
        FREE_FRAME(free_frames_table[del_frame_ind]);
        // On écrase l'entrée de niveau 2
        *(del_entry+ind_2) &= 0x0;
    } 
}

void* sys_mmap(uint32_t size)
{
    // Déplacement du registre contenant size
    __asm("mov r1, r0");
    // SWI
    SWI(SCI_MMAP);

    uint32_t vAddr;
    __asm("mov %0, r0" : "=r"(vAddr));

    return (void*)(vAddr);
}

void do_sys_mmap(pcb_s * context)
{
    // Allocation grâceầ la methode privee d'allocation memoire
    uint32_t * virtualAddress = (uint32_t*) vmem_alloc_in_userland(current_process, context->registres[1]);

    // Retour dans r0 de l'adresse virtuelle du debut du bloc alloué
    context->registres[0] = (uint32_t)(virtualAddress);
}

void sys_munmap(void * addr, uint32_t size)
{
    // Déplacement des registres contenant addr et size
    __asm("mov r2, r1\n\t"
          "mov r1, r0");
    // SWI
    SWI(SCI_MUNMAP);

    return;
}

void do_sys_munmap(pcb_s * context)
{
    // Reconstruction de addr
    uint8_t * addr = (uint8_t*)(context->registres[1]);
    // Reconstruction de size
    uint32_t size = context->registres[2];
    // Appel à vmem free
    vmem_free(addr, context, size);
}

// Fonctions données par le sujet --------------------------------

uint32_t vmem_translate(uint32_t va, uint32_t table_base)
{
    uint32_t pa; /* The result */
    /* 1st and 2nd table addresses */
    uint32_t second_level_table;
    /* Indexes */
    uint32_t first_level_index;
    uint32_t second_level_index;
    uint32_t page_index;
    /* Descriptors */
    uint32_t first_level_descriptor;
    uint32_t* first_level_descriptor_address;
    uint32_t second_level_descriptor;
    uint32_t* second_level_descriptor_address;

    table_base = table_base & 0xFFFFC000;

    /* Indexes*/
    first_level_index = (va >> 20);
    second_level_index = ((va << 12) >> 24);
    page_index = (va & 0x00000FFF);
    /* First level descriptor */
    first_level_descriptor_address = (uint32_t*) (table_base | (first_level_index << 2));
    first_level_descriptor = *(first_level_descriptor_address);
    /* Translation fault*/
    if (! (first_level_descriptor & 0x3)) {
        return (uint32_t) FORBIDDEN_ADDRESS;
    }
    /* Second level descriptor */
    second_level_table = first_level_descriptor & 0xFFFFFC00;
    second_level_descriptor_address = (uint32_t*) (second_level_table | (second_level_index << 2));
    second_level_descriptor = *((uint32_t*) second_level_descriptor_address);
    /* Translation fault*/
    if (! (second_level_descriptor & 0x3)) {
        return (uint32_t) FORBIDDEN_ADDRESS;
    }
    /* Physical address */
    pa = (second_level_descriptor & 0xFFFFF000) | page_index;
    return pa;
}

uint32_t vmem_translate_ps(uint32_t va, pcb_s* process)
{
    uint32_t pa; /* The result */
    /* 1st and 2nd table addresses */
    uint32_t table_base;
    uint32_t second_level_table;
    /* Indexes */
    uint32_t first_level_index;
    uint32_t second_level_index;
    uint32_t page_index;
    /* Descriptors */
    uint32_t first_level_descriptor;
    uint32_t* first_level_descriptor_address;
    uint32_t second_level_descriptor;
    uint32_t* second_level_descriptor_address;
    if (process == 0x0)
    {
        __asm("mrc p15, 0, %[tb], c2, c0, 0" : [tb] "=r"(table_base));
    }
    else
    {
        table_base = (uint32_t) process->page_table;
    }
    table_base = table_base & 0xFFFFC000;
    /* Indexes*/
    first_level_index = (va >> 20);
    second_level_index = ((va << 12) >> 24);
    page_index = (va & 0x00000FFF);
    /* First level descriptor */
    first_level_descriptor_address = (uint32_t*) (table_base | (first_level_index << 2));
    first_level_descriptor = *(first_level_descriptor_address);
    /* Translation fault*/
    if (! (first_level_descriptor & 0x3)) {
        return (uint32_t) FORBIDDEN_ADDRESS;
    }
    /* Second level descriptor */
    second_level_table = first_level_descriptor & 0xFFFFFC00;
    second_level_descriptor_address = (uint32_t*) (second_level_table | (second_level_index << 2));
    second_level_descriptor = *((uint32_t*) second_level_descriptor_address);
    /* Translation fault*/
    if (! (second_level_descriptor & 0x3)) {
        return (uint32_t) FORBIDDEN_ADDRESS;
    }
    /* Physical address */
    pa = (second_level_descriptor & 0xFFFFF000) | page_index;
    return pa;
}

void start_mmu_C(void) 
{
    uint32_t control;
    __asm("mcr p15, 0, %[zero], c1, c0, 0" : : [zero] "r"(0));
    //Disable cache
    __asm("mcr p15, 0, r0, c7, c7, 0");
    //Invalidate cache (data and instructions) */
    __asm("mcr p15, 0, r0, c8, c7, 0");
    //Invalidate TLB entries
    /* Enable ARMv6 MMU features (disable sub-page AP) */
    control = (1 << 23) | (1 << 15) | (1 << 4) | 1;
    /* Invalidate the translation lookaside buffer (TLB) */
    __asm volatile("mcr p15, 0, %[data], c8, c7, 0" : : [data] "r" (0));
    /* Write control register */
    __asm volatile("mcr p15, 0, %[control], c1, c0, 0" : : [control] "r" (control));
}

void configure_mmu_C(uint32_t translation_base) 
{
    uint32_t pt_addr = translation_base;
    /* Translation table 0 */
    __asm volatile("mcr p15, 0, %[addr], c2, c0, 0" : : [addr] "r" (pt_addr));
    /* Translation table 1 */
    __asm volatile("mcr p15, 0, %[addr], c2, c0, 1" : : [addr] "r" (pt_addr));
    /* Use translation table 0 for everything */
    __asm volatile("mcr p15, 0, %[n], c2, c0, 2" : : [n] "r" (0));
    /* Set Domain 0 ACL to "Manager", not enforcing memory permissions
     * Every mapped section/page is in domain 0
     */
    __asm volatile("mcr p15, 0, %[r], c3, c0, 0" : : [r] "r" (0x3));
}

void __attribute__((naked)) data_handler(void)
{
    int error;
    // On récupère le code de l'erreur
    __asm("mcr p15, 0, %0, c5, c0, 0" : "=r"(error));
    // On masque sur les 4 bits de poids faible
    error = error & 0x8; 
    // On identifie l'erreur
    switch(error)
    {
        case TRANSLATION_FAULT: break;
        case ACCESS_FAULT:      break;        
        case PRIVILEDGES_FAULT: break;
    }
    // On termine l'execution du noyau
    terminate_kernel();
}