#ifndef DAILY_H
#define DAILY_H
namespace daily {
    /**
    * @brief Bitmasks for each daily check
    */
    enum check {
        /**
	      * @brief User was spotted online at least once
         */
        c_wasOnline           = (1 << 0),

        /**
	      * @brief User was spotted in call at least once
         */
        c_wasInCall           = (1 << 1),

        /**
	      * @brief Just a fancy way of zeroing to show all the different dailies
         */
        c_emptyCheck          = c_wasOnline & c_wasInCall
    };

    /**
    * @brief How much your balance is changed by each check
    */
    enum delta {
        /**
	      * @brief User was spotted online at least once
         */
        d_wasOnline           = 1,

        /**
	      * @brief User was spotted in call at least once
         */
        d_wasInCall           = 1,
    };
}
#endif