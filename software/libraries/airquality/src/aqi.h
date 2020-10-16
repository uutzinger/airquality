/**
 * Qir Quality Index (AQI) header file
 *
 * Copyright (c) March 2019, Paul van Haastrecht
 *
 * All rights reserved.
 *
 * The library can receive PM2.5 & PM10 data samples. These will be
 * collected by hour. After an hour the average values of the previous hour
 * will be counted during the day. During-hour and during-day values
 * are stored in RAM. After a day (24 hours) the averages of the daily
 * values will be counted and combined with the values stored from previous
 * days. These after day-values are stored in NVRAM so they are retained
 * after an power-off.
 *
 * More info : https://en.wikipedia.org/wiki/Air_quality_index
 *
 * Development environment specifics:
 * Arduino IDE 1.9
 *
 * ================ Disclaimer ===================================
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 **********************************************************************
 * Version 1.0 / March 2019
 * - Initial version by paulvha
 *
 * version 1.0.1 / March 2019
 * -  Added base-option for PM2.5 and PM10
 */

#ifndef AQI_H
#define AQI_H

#include "Arduino.h"
#include "EEPROM.h"
#include "printf.h"
#include "aqi_region.h"           // contains the limit definitions per region

/* Holds the after-day values as stored in NVRAM
 * Can be returned as part of the GetNv() call */
struct AQI_NVRAM
{
    uint8_t  _region;             // region is selected
    uint16_t _cnt;                // number of days in total
    float    _25um;               // total 2.5um mass values
    float    _10um;               // total 10um mass values
    float    _25um_prev;          // previous day value
    float    _10um_prev;          // previous day value
    float    _25um_max;           // maximum 25um during previous day
    float    _10um_max;           // maximum 10um during previous day

    uint8_t  _hrly_bnd_25um[5];   // 2.5um hourly count(EU) (previous day only)
    uint8_t  _hrly_bnd_10um[5];   // 10um hourly count (EU) (previous day only)
    uint8_t  _daily_bnd_25um[5];  // 2.5um daily count (EU)
    uint8_t  _daily_bnd_10um[5];  // 10um daily count  (EU)
};

/* Holds the Air Quality Index (AQI) information
 * return values from GetAqi() */
struct AQI_info {

    float   aqi_index;            // AQI calculated
    char    aqi_name[25];         // AQI corresponding name
    bool    base ;                // HISTORY : Long term / YESTERDAY previous day PM25 and PM10 used
    bool    aqi_indicator;        // PM25: AQI driven by PM2.5 value, PM10:  AQI driven by PM10
    float   aqi_pmvalue;          // the value used either PM25 or PM10
    uint8_t aqi_bnd;              // pollution band number
    float   aqi_bnd_low;          // pollution band low value
    float   aqi_bnd_high;         // pollution band high value

    struct AQI_NVRAM nv;          // nvram values for reference
};

#define YESTERDAY true
#define HISTORY false
#define PM25 true
#define PM10 false

/* Holds the within-hour and within-day values information.
 * These are volatile and are stored in RAM
 * returns with the ReadRam() call */
struct AQI_stats
{
    // hourly
    float    _within_hr_25um;     // total 2.5um mass values
    float    _within_hr_10um;     // total 10um mass values
    uint16_t _within_hr_cnt;      // number of samples in hourly total
    uint8_t  _hrly_bnd_25um[5];   // 2.5um hourly band count during day(EU)
    uint8_t  _hrly_bnd_10um[5];   // 10um hourly band count during day (EU)

    // daily statistics
    float    _daily_25um;         // total PM2.5 mass values
    float    _daily_10um;         // total PM10  mass values
    float    _daily_25um_max;     // maximum PM2.5 hourly during day
    float    _daily_10um_max;     // maximum PM10  hourly during day
    uint8_t  _daily_cnt;          // number of hours in daily total (max 24)
    uint8_t  _daily_offset;       // set the start hour offset
    long     _start_hour;         // save starting moment

};

/* needed for conversion float IEE754 */
union {
    byte array[4];
    float value;
} FloatToBYTE;


/**
 * NVRAM information :
 *
 * start address + 0    _region
 * start address + 1    _25um total
 * start address + 5    _10um total
 * start address + 9    _25um_prev;        // previous day value
 * start address + 13   _10um_prev;        // previous day
 * start address + 17   _25um_max          // previous day
 * start address + 21   _10um_max          // previous day
 * start address + 25   _hrly_bnd_25um[5]  // 2.5um hourly band count (EU)
 * start address + 30   _hrly_bnd_10um[5]  // 10um hourly band count (EU)
 * start address + 35   _daily_bnd_25um[5] // 2.5um daily band count (EU)
 * start address + 40   _daily_bnd_10um[5] // 10um daily band count (EU)
 * start address + 45   _cnt
 * start address + 47    END
 */

#define StartAddrNV 0                       // start position
#define AQISIZE 47                          // length
#define LastAddrNV  StartAddrNV + AQISIZE   // last position

class AQI
{
  public:

    AQI(void);

    /**
     * @brief : update the NVRAM statistics
     * @param nv : structure with data to preset OR NULL to clear NVRAM
     */
    void SetStats(struct AQI_NVRAM *nv);

    /**
     * @brief : update the region to compare against
     * @param : region identifier (see aqi_region.h)
     */
    void SetRegion(region_t region);

    /**
     * @brief : store any reading done. will be grouped by the hour
     * @param um25 : measured value of PM2.5
     * @param um10 : measured value of PM10
     */
    void Capture(float um25, float um10);

    /**
     * @brief : calculate the AQI status based on the stored values
     * @param r : structure to store return values
     * @param base :
     *  YESTERDAY : will use the PM25 and PM10 from previous day to compare
     *  HISTORY : will use the long term PM25 and PM10 values to compare
     *
     * This will include a generated air quality index information
     * depending on the selected region.
     *
     * @return
     *  false  : no values stored (yet) or no area : no reporting possible
     *  true   : succesfully completed
     */
    bool GetAqi(struct AQI_info *r, bool base = HISTORY);

    /**
     * @brief : read current statistics stored in volatile RAM
     * @param r : pointer to structure to return values.
     */
    void ReadRam( struct AQI_stats *r);

    /**
     * @brief : get the raw information of the NVRAM stored values
     * @param r : place to store the AQI-NVRAM content
     */
    void GetNv(struct AQI_NVRAM *r);

    /**
     * @brief : force an update of RAM to NVRAM
     *
     * This will only update if at least one full hour has been captured
     * to be used when closing down and wanting to save current values
     */
    bool ForceUpdate();

    /**
     * @brief : set the hour offset in current day to enable aligning to local clock
     * @param hour : the hour offset to set (between 0 and 23)
     *
     * will reset the current day stored values.
     */
    bool SetHour(uint8_t hour);

    /**
     * @brief : get the current hour of the day being captured (0 -23)
     */
    uint8_t GetHour();

  private:

    // start of day
    bool _day_started;

    /**
     * holds within-hour and within-daily statistics
     */
    struct AQI_stats s;

    /**
     * @brief : initialize values for new day
     */
    void InitDay();

    /**
     * @brief : store hourly captured values within a day in RAM
     */
    void WithinDay();

    /**
     * @brief : store the values after one day
     *
     * The values are stored in nvram so they are not lost when the
     * system is reset / rebooted
     */
    void AfterDay();

    /**
     * @brief : supporting to routines to read from NVRAM
     */
    void read_nvram(struct AQI_NVRAM *nv);
    float read_nv_float(uint8_t addr);

    /**
     * @brief : supporting to routines to write to NVRAM
     */
    void write_nvram(struct AQI_NVRAM *nv);
    void write_nv_float(uint8_t addr, float val);
};
#endif /* AQI_H */
