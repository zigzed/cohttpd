/**
 * create: 2016-09-27
 * author: mr.wclong@yahoo.com
 */
#include "semops.hpp"
#include "errors.hpp"
#include <sys/sem.h>
#include <string.h>
#include <errno.h>

#include <stdexcept>

namespace coh {
    
    semops::semops(int value)
        : semid_(-1)
    {
        int semid_ = semget(IPC_PRIVATE, 1, IPC_CREAT | 0666);
        if(semid_ == -1) {
            int code = errno;
            THROW_EXCEPT(std::runtime_error, "can't create semaphore for accept: %d, %s",
                         code, strerror(code));
        }
        
        union {
            int                 val;
            struct semid_ds*    buf;
            unsigned short*     array;
            struct seminfo*     __buf;
        } sem_v;
        sem_v.val   = value;
        int rc = semctl(semid_, 0, SETVAL, sem_v);
        if(rc == -1) {
            int code = errno;
            THROW_EXCEPT(std::runtime_error, "can't setval semaphore for accept: %d, %s",
                         code, strerror(code));
        }
    }
    
    semops::~semops()
    {
        if(semid_ != -1) {
            semctl(semid_, 0, IPC_RMID);
            semid_ = -1;
        }
    }
    
    bool semops::wait()
    {
        sembuf sb;
        sb.sem_num  = 0;
        sb.sem_op   = -1;
        sb.sem_flg  = 0;
        int rc = semop(semid_, &sb, 1);
        return rc == 0;
    }
    
    void semops::wake()
    {
        sembuf sb;
        sb.sem_num  = 0;
        sb.sem_op   = 1;
        sb.sem_flg  = 0;
        semop(semid_, &sb, 1);
    }
    
}