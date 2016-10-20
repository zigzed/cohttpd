/**
 * create: 2016-09-27
 * author: mr.wclong@yahoo.com
 */
#ifndef COH_SEMOPS_HPP
#define COH_SEMOPS_HPP

namespace coh {
    
    class semops {
    public:
        explicit semops(int value);
        ~semops();
        void wake();
        bool wait();
    private:
        int semid_;
    };
    
}

#endif