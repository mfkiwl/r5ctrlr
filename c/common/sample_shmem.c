//
// R5 integrated controller 
// ZCU102 + ADI CN0585/CN0584
//
// IRQs from PL and 
// interproc communications with A53/linux
//
//////////////////////////////////////////
//
// this is the setup of a shared memory
// for ADC samples
//

#include "sample_shmem.h"

// ##########  globals  #######################

#ifdef ARMR5
// ########### R5 side

const metal_phys_addr_t sample_shmem_phys = SAMPLE_SHM_BADDR;

#define DEFAULT_PAGE_SHIFT (-1UL)
#define DEFAULT_PAGE_MASK  (-1UL)

static struct metal_device sample_shmem_table=
  {
  .name = SAMPLE_SHM_DEVNAME,
  .bus = NULL,
  .num_regions = 1,
  .regions = 
    {
      {
      .virt = (void *)SAMPLE_SHM_BADDR,
      .physmap = &sample_shmem_phys,
      .size = SAMPLE_SHM_SIZE,
      .page_shift = DEFAULT_PAGE_SHIFT,
      .page_mask = DEFAULT_PAGE_MASK,
      .mem_flags = NORM_SHARED_NCACHE | PRIV_RW_USER_RW,
      .ops = {NULL},
      }
    },
  .node = {NULL},
  .irq_num = 0,
  .irq_info = NULL,
  };

#endif

struct metal_device *sample_shmem_dev = NULL;
struct metal_io_region *sample_shmem_io = NULL;


// ##########  implementation  ################

int InitSampleShmem(void)
  {
  int status;

  #ifdef ARMR5
  // ########### R5 side
  status=metal_register_generic_device(&sample_shmem_table);
  if(status!=0)
    return XST_FAILURE;
  #endif

  status=metal_device_open(SAMPLE_SHM_BUSNAME, SAMPLE_SHM_DEVNAME, &sample_shmem_dev);
  if(status!=0)
    return XST_FAILURE;
  
  sample_shmem_io=metal_device_io_region(sample_shmem_dev,0);
  if(sample_shmem_io==NULL)
    return XST_FAILURE;

  return XST_SUCCESS;
  }
