/**
 *
 * Copyright (c) March 2019, Paul van Haastrecht
 *
 * All rights reserved.
 *
 * The library can receive PM2.5 & PM10 data samples. These will be
 * collected by hour. After an hour the average values of the hour
 * will be counted during the day. During-hour and during-day values
 * are stored in RAM. After a day (24 hours) the averages of the daily
 * values will be counted and combined with the values stored from previous
 * days. These after day-values are stored in NVRAM so they are retained
 * after an power-off.
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

#include "aqi.h"

/**
 * @brief constructor and initialize variables
 */
AQI::AQI(void)
{
    // reset starting moment
    s._start_hour = 0;

    // reset start of day
    InitDay();
}

////////////////////////////////////////////////////////////////
//  initialize the data                                       //
////////////////////////////////////////////////////////////////

/**
 * @brief : initialize values for new day
 */
void AQI::InitDay()
{
    uint8_t x;

    s._daily_25um = 0;
    s._daily_10um = 0;
    s._daily_cnt = 0;
    s._daily_offset = 0;

    s._daily_25um_max = 0;
    s._daily_10um_max = 0;

    for(x = 0 ; x < 5 ; x++){
        s._hrly_bnd_25um[x] = 0;
        s._hrly_bnd_10um[x] = 0;
    }

    _day_started = true;
}

////////////////////////////////////////////////////////////////
//  manual commands to changes to the data                    //
////////////////////////////////////////////////////////////////

/**
 * @brief : update the NVRAM area
 * @param nv : structure with data to write OR NULL to clear NVRAM
 */
void AQI::SetStats(struct AQI_NVRAM *nv)
{
    uint8_t i;

    // if struct is not NULL use the content
    if (nv != NULL)   write_nvram(nv);

    // else clear all the novram statistics EXCEPT area code
    else {

        for (i = StartAddrNV+1 ; i < LastAddrNV ; i++)
                EEPROM.write(i, 0);

        // store in flash
        EEPROM.commit();
    }
}

/**
 * @brief : update the region code
 * @param : region identifier
 */
void AQI::SetRegion(region_t region)
{
    // save to nvram
    EEPROM.write(StartAddrNV, (uint8_t) region);

    // store in flash
    EEPROM.commit();
}

/**
 * @brief : force an update of RAM to NVRAM
 *
 * This will only update if at least one full hour has been captured
 * to be used when terminating the measurements and wanting to save
 * current values
 */
bool AQI::ForceUpdate()
{
    if (s._daily_cnt == 0) return(false);

    // perform an end of day cycle
    AfterDay();

    // reset to start of hour
    s._start_hour = 0;

    // reset start of day
    InitDay();

    return(true);
}

/**
 * @brief : set the hour count on current day to enable aligning to local clock
 * @param hour : the hours to offset (between 0 and 23)
 *
 * will reset the current day stored values.
 *
 * @return
 *  false if incorrect hour provided
 *  true if OK
 */
bool AQI::SetHour(uint8_t hour)
{
    if (hour < 0 || hour > 23 || hour + s._daily_cnt > 23 ) return(false);

    // reset current day counters
    InitDay();

    s._daily_offset = hour;

    return(true);
}

/**
 * @brief : get the current hour being captured
 */
uint8_t AQI::GetHour()
{
    return(s._daily_cnt + s._daily_offset);
}

////////////////////////////////////////////////////////////////
//  capture the data                                          //
////////////////////////////////////////////////////////////////

/**
 * @brief : store measurement samples. They will be grouped by the hour
 * @param um25 : measured value of PM2.5
 * @param um10 : measured value of PM10
 */
void AQI::Capture(float um25, float um10)
{
    // init within-hour information (new hour)
    if (s._start_hour == 0) {
        s._within_hr_25um = 0;
        s._within_hr_10um = 0;
        s._within_hr_cnt = 0;

        s._start_hour = millis();
    }

    // No real value provided
    if (um25 <= 0 || um10 <= 0 ) return;

    // add within-hour sample
    s._within_hr_25um += um25;
    s._within_hr_10um += um10;
    s._within_hr_cnt++;

    /* check that 60 minutes (60 *60*1000 = 360000mS) have passed OR
     * check whether millis() has made a round turn (after 49 days) */
    if (millis() - s._start_hour >= 3600000 || millis() - s._start_hour < 0)
    {
        // add to daily statistics.
        WithinDay();

        // reset within_hour statistics
        s._start_hour = 0;
    }
}

/**
 * @brief : calculate and store the values within a day
 */
void AQI::WithinDay()
{
    float tmp, tmp1;
    uint8_t x;

    // init (new daystart)
    if (! _day_started) InitDay();

    if( s._within_hr_cnt == 0) return;

    // capture / add average previous hour value
    tmp  = s._within_hr_25um / s._within_hr_cnt;
    tmp1 = s._within_hr_10um / s._within_hr_cnt;
    s._daily_25um += tmp;
    s._daily_10um += tmp1;
    s._daily_cnt++;

    // capture the max value of an hour during the day
    if (tmp  > s._daily_25um_max) s._daily_25um_max = tmp;
    if (tmp1 > s._daily_10um_max) s._daily_10um_max = tmp1;

    //************ for EU only ****************
    for(x = 0 ; x < 5 ; x++)
    {
       // determine the 1 hour polutions band for 2.5um and 10um
       if (tmp > AQI_CAQI_hrly[x]._25um_low &&  tmp <= AQI_CAQI_hrly[x]._25um_high)
            s._hrly_bnd_25um[x] += 1;

       if (tmp1 > AQI_CAQI_hrly[x]._10um_low && tmp1 <= AQI_CAQI_hrly[x]._10um_high)
            s._hrly_bnd_10um[x] += 1;
    }
    //************ end EU ONLY ********************

    if (s._daily_cnt + s._daily_offset > 23)        // needs to be 23  !!!!!
    {
        AfterDay();

        // reset statistics
        _day_started = false;
    }
}

/**
 * @brief : store the values after one day
 *
 * The values are stored in novram so they are not lost when the
 * system is reset / rebooted
 */
void AQI::AfterDay()
{
    struct AQI_NVRAM nv;
    uint8_t x;
    uint8_t stat_25um = 0;
    uint8_t stat_10um = 0;
    float tmp, tmp1;

    // read novram : value / max_value + count_days + max_counts etc
    read_nvram(&nv);

    // add average previous day_value to total
    tmp  = s._daily_25um/s._daily_cnt;
    tmp1 = s._daily_10um/s._daily_cnt;

    nv._25um += tmp;
    nv._10um += tmp1;
    nv._cnt++;

    // update values during previous day
    nv._25um_prev = tmp;
    nv._10um_prev = tmp1;

    // update max hourly value during previous day
    nv._25um_max = s._daily_25um_max;
    nv._10um_max = s._daily_10um_max;

    //************ valid for EU only ****************
    for(x = 0 ; x < 5 ; x++)
    {
       // get the hourly bandcount
       nv._hrly_bnd_25um[x] = s._hrly_bnd_25um[x];
       nv._hrly_bnd_10um[x] = s._hrly_bnd_10um[x];

       // check & add from previous day the DAILY polutions band count
       if (tmp > AQI_CAQI_daily[x]._25um_low &&  tmp <= AQI_CAQI_daily[x]._25um_high)
            nv._daily_bnd_25um[x] += 1;

       if (tmp1 > AQI_CAQI_daily[x]._10um_low && tmp1 <= AQI_CAQI_daily[x]._10um_high)
            nv._daily_bnd_10um[x] += 1;
    }
    //************end EU ONLY ********************

    // save to novram : value / max_value + count_days + max_count_days
    write_nvram(&nv);
}

///////////////////////////////////////////////////////////////
//  RAM routines                                             //
///////////////////////////////////////////////////////////////

/**
 * @brief : read current statistics stored in RAM
 * @param r :pointer to structure to return values.
 */
void AQI::ReadRam( struct AQI_stats *r)
{
    uint8_t x;

    if (r == NULL) return;

    // return hourly & daily capture information so far if provided
    r->_within_hr_25um = s._within_hr_25um;    // only within hour valid
    r->_within_hr_10um = s._within_hr_10um;
    r->_within_hr_cnt  = s._within_hr_cnt;
    r->_start_hour = s._start_hour;

    r->_daily_25um = s._daily_25um;             // accumulated today
    r->_daily_10um = s._daily_10um;
    r->_daily_cnt  = s._daily_cnt;
    r->_daily_offset = s._daily_offset;

    r->_daily_25um_max = s._daily_25um_max;     // max hourly value during day
    r->_daily_10um_max = s._daily_10um_max;

    //******** valid for EU only **********
    for (x =0 ; x < 5 ; x++)
    {
        r->_hrly_bnd_25um[x]  = s._hrly_bnd_25um[x];    // 2.5um hourly count
        r->_hrly_bnd_10um[x]  = s._hrly_bnd_10um[x];    // 10um hourly count
    }
}

//////////////////////////////////////////////////////////////////
//  NVRAM  routines                                             //
//////////////////////////////////////////////////////////////////

/**
 * @brief : translate 4 bytes to float IEEE754
 * @param x : offset in nvram
 *
 * return : float number
 */
float AQI::read_nv_float(uint8_t addr)
{
    FloatToBYTE conv;
    uint8_t i;

    for (i = 0; i < 4; i++) conv.array[3-i] = EEPROM.read(addr++);

    return conv.value;
}

/**
 * @brief : write a float value to NVRAM
 * @param addr : address in NVRAM to start
 * @param val  : float value to store
 *
 * Will take 4 address places.
 */
void AQI::write_nv_float(uint8_t addr, float val)
{
    FloatToBYTE conv;
    uint8_t i;

    conv.value = val;
    for (i = 0; i < 4; i++) EEPROM.write(addr++,conv.array[3-i]);
}

/**
 * @brief : read the values from NVRAM
 * @param nv : to store the read values
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
 *
 */
void AQI::read_nvram(struct AQI_NVRAM *nv)
{
    uint8_t x;
    uint8_t addr = StartAddrNV;

    nv->_region = EEPROM.read(addr++);

    nv->_25um = read_nv_float(addr);
    addr += 4;

    nv->_10um = read_nv_float(addr);
    addr += 4;

    nv->_25um_prev = read_nv_float(addr);
    addr += 4;

    nv->_10um_prev = read_nv_float(addr);
    addr += 4;

    nv->_25um_max = read_nv_float(addr);
    addr += 4;

    nv->_10um_max = read_nv_float(addr);
    addr += 4;

    for (x = 0 ; x < 5 ; x++)
        nv->_hrly_bnd_25um[x] = EEPROM.read(addr++);

    for (x = 0 ; x < 5 ; x++)
        nv->_hrly_bnd_10um[x] = EEPROM.read(addr++);

    for (x = 0 ; x < 5 ; x++)
        nv->_daily_bnd_25um[x] = EEPROM.read(addr++);

    for (x = 0 ; x < 5 ; x++)
        nv->_daily_bnd_10um[x] = EEPROM.read(addr++);

    nv->_cnt = EEPROM.read(addr++);
    nv->_cnt = nv->_cnt << 8;
    nv->_cnt |= EEPROM.read(addr++);
}

/**
 * @brief : write the values to NVRAM
 * @param nv : values to write
 *
 *
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
void AQI::write_nvram(struct AQI_NVRAM *nv)
{
    uint8_t x;
    uint8_t addr = StartAddrNV;

    EEPROM.write(addr++, nv->_region);

    write_nv_float(addr, nv->_25um);
    addr += 4;

    write_nv_float(addr, nv->_10um);
    addr += 4;

    write_nv_float(addr, nv->_25um_prev);
    addr += 4;

    write_nv_float(addr, nv->_10um_prev);
    addr += 4;

    write_nv_float(addr, nv->_25um_max);
    addr += 4;

    write_nv_float(addr, nv->_10um_max);
    addr += 4;

    for (x = 0 ; x < 5 ; x++)
        EEPROM.write(addr++, nv->_hrly_bnd_25um[x]);

    for (x = 0 ; x < 5 ; x++)
        EEPROM.write(addr++, nv->_hrly_bnd_10um[x]);

    for (x = 0 ; x < 5 ; x++)
        EEPROM.write(addr++, nv->_daily_bnd_25um[x]);

    for (x = 0 ; x < 5 ; x++)
        EEPROM.write(addr++, nv->_daily_bnd_10um[x]);

    EEPROM.write(addr++,(nv->_cnt >> 8) & 0xff);
    EEPROM.write(addr++,nv->_cnt & 0xff);

    // store all the changes in NVRAM/flash
    EEPROM.commit();
}

//////////////////////////////////////////////////////////////////
//  interprete results                                          //
//////////////////////////////////////////////////////////////////

/**
 * @brief : calculate the AQI status based on the stored values
 * @param r : structure to store return values
 * @param base :
 *  YESTERDAY : will use the PM25 and PM10 from previous day to compare
 *  HISTORY : will use the long term PM25 and PM10 values to compare
 *
 * @return
 *  false  : no values stored (yet) or no area : no reporting possible
 *  true   : succesfully completed
 */
bool AQI::GetAqi(struct AQI_info *r, bool base)
{
    uint8_t x;
    uint8_t stat_25um = 0;
    uint8_t stat_10um = 0;
    float aqi;
    float ref_PM25, ref_PM10;

    if (r == NULL) return(false);

    // read novram values
    read_nvram(&r->nv);

    // if none values / days or no area selected
    if (r->nv._cnt == 0 || r->nv._region == 0)  return(false);

    // indicat the used base values
    r->base = base;

    if (base == YESTERDAY)
    {
        ref_PM25 = r->nv._25um_prev;
        ref_PM10 = r->nv._10um_prev;
    }
    else
    {
        ref_PM25 = r->nv._25um / r->nv._cnt;
        ref_PM10 = r->nv._10um / r->nv._cnt;
    }

    // ***** calculate AQI depending on area  *******************

    if (r->nv._region == USA)
    {
        // the USA uses the WORST situation of either 2,5um or 10um
        for (x = 0; x <  sizeof(AQI_USA) / sizeof(struct AQI_area); x++)
        {
            // find the situation
            if (ref_PM25 >= AQI_USA[x]._25um_low && ref_PM25 <= AQI_USA[x]._25um_high)
            {
                stat_25um = x;
            }

            if (ref_PM10 >= AQI_USA[x]._10um_low && ref_PM10 <= AQI_USA[x]._10um_high)
            {
                stat_10um = x;
            }
        }

        if (stat_25um == stat_10um || stat_25um > stat_10um )
        {
            // use the 2.5um to calculate the AQI
            aqi = (AQI_USA[stat_25um]._val_high - AQI_USA[stat_25um]._val_low) / (AQI_USA[stat_25um]._25um_high -AQI_USA[stat_25um]._25um_low);
            aqi = aqi * (ref_PM25 - AQI_USA[stat_25um]._25um_low) + AQI_USA[stat_25um]._val_low;

            strcpy(r->aqi_name, AQI_USA[stat_25um].cat_name);
            r->aqi_bnd_high = AQI_USA[stat_25um]._val_high;
            r->aqi_bnd_low = AQI_USA[stat_25um]._val_low;
            r->aqi_bnd = stat_25um+1;
            r->aqi_indicator = PM25;
        }

        else
        {
            // use the 10um situation as it is worse than 2.5um
            aqi = (AQI_USA[stat_10um]._val_high - AQI_USA[stat_10um]._val_low) / (AQI_USA[stat_10um]._10um_high -AQI_USA[stat_10um]._10um_low);
            aqi = aqi * (ref_PM10 - AQI_USA[stat_10um]._10um_low) + AQI_USA[stat_10um]._val_low;

            strcpy(r->aqi_name, AQI_USA[stat_10um].cat_name);
            r->aqi_bnd_high = AQI_USA[stat_10um]._val_high;
            r->aqi_bnd_low = AQI_USA[stat_10um]._val_low;
            r->aqi_bnd = stat_10um+1;
            r->aqi_indicator = PM10;
        }

        r->aqi_index = aqi;

    }
    else if (r->nv._region == EUROPE1)
    {
        /* The EU uses the WORST situation of a core pollutants
         *
         * According to the definition : The calculation is based on three
         * pollutants of major concern: PM 10 , NO2 ,O3. It can also take
         * the pollutants PM 2.5 , CO and SO2 into account if these data are also available.
         *
         * This calculation only takes PM10 from previous day into account to calculate the Air Quality Index
         * The other information is made available to the user program */

        for (x = 0; x < sizeof(AQI_CAQI_daily) / sizeof(struct AQI_area); x++)
        {
            if (ref_PM10 > AQI_CAQI_daily[x]._10um_low && ref_PM10 <= AQI_CAQI_daily[x]._10um_high)
            {
                stat_10um = x;
            }
        }

        aqi = (AQI_CAQI_daily[stat_10um]._val_high - AQI_CAQI_daily[stat_10um]._val_low) / (AQI_CAQI_daily[stat_10um]._10um_high - AQI_CAQI_daily[stat_10um]._10um_low);
        aqi = aqi * (ref_PM10 - AQI_CAQI_daily[stat_10um]._10um_low) + AQI_CAQI_daily[stat_10um]._val_low;

        strcpy(r->aqi_name, AQI_CAQI_daily[stat_10um].cat_name);
        r->aqi_bnd_high = AQI_CAQI_daily[stat_10um]._val_high;
        r->aqi_bnd_low = AQI_CAQI_daily[stat_10um]._val_low;
        r->aqi_bnd = stat_10um+1;
        r->aqi_indicator = PM10;
        r->aqi_index = aqi;
    }
    else if (r->nv._region == EUROPE2)
    {
        /* The EU uses the WORST situation of a core pollutants
         *
         * According to the definition : The calculation is based on three
         * pollutants of major concern: PM 10 , NO2 ,O3. It can also take
         * the pollutants PM 2.5 , CO and SO2 into account if these data are also available.
         *
         * This calculation only takes PM2.5 from previous day into account to calculate the Air Quality Index
         * The other information is made available to the user program */

        for (x = 0; x < sizeof(AQI_CAQI_daily) / sizeof(struct AQI_area); x++)
        {
            if (ref_PM25 > AQI_CAQI_daily[x]._25um_low && ref_PM25 <= AQI_CAQI_daily[x]._25um_high)
            {
                stat_25um = x;
            }
        }

        aqi = (AQI_CAQI_daily[stat_25um]._val_high - AQI_CAQI_daily[stat_25um]._val_low) / (AQI_CAQI_daily[stat_25um]._25um_high -AQI_CAQI_daily[stat_25um]._25um_low);
        aqi = aqi * (ref_PM25 - AQI_CAQI_daily[stat_25um]._25um_low) + AQI_CAQI_daily[stat_25um]._val_low;

        strcpy(r->aqi_name, AQI_CAQI_daily[stat_25um].cat_name);
        r->aqi_bnd_high = AQI_CAQI_daily[stat_25um]._val_high;
        r->aqi_bnd_low = AQI_CAQI_daily[stat_25um]._val_low;
        r->aqi_bnd = stat_25um+1;
        r->aqi_indicator = PM25;
        r->aqi_index = aqi;
    }
    else if (r->nv._region == UK)
    {
        // use the WORST situation of either PM2.5 or PM10
        for (x = 0; x < sizeof(UK_daily) / sizeof(struct AQI_area); x++)
        {
            // find the situation
            if (ref_PM25 > UK_daily[x]._25um_low && ref_PM25 <= UK_daily[x]._25um_high)
            {
                stat_25um = x;
            }

            if (ref_PM10 > UK_daily[x]._10um_low && ref_PM10 <= UK_daily[x]._10um_high)
            {
                stat_10um = x;
            }
        }

        if (stat_25um == stat_10um || stat_25um > stat_10um ) {
            // use the 2.5um as the AQI
            r->aqi_index = ref_PM25;
            strcpy(r->aqi_name, UK_daily[stat_25um].cat_name);
            r->aqi_bnd_high = UK_daily[x]._25um_high;
            r->aqi_bnd_low = UK_daily[x]._25um_low;
            r->aqi_bnd = stat_25um+1;
            r->aqi_indicator = PM25;
        }
        else {
            // use the 10um as the AQI
            r->aqi_index = ref_PM10;
            strcpy(r->aqi_name, UK_daily[stat_10um].cat_name);
            r->aqi_bnd_high = UK_daily[x]._10um_high;
            r->aqi_bnd_low = UK_daily[x]._10um_low;
            r->aqi_bnd = stat_10um+1;
            r->aqi_indicator = PM10;
        }
    }
    else if (r->nv._region == INDIA)
    {
        // use the WORST situation of either PM2.5 or PM10
        for (x = 0; x < sizeof(AQI_INDIA) / sizeof(struct AQI_area); x++)
        {
            // find the situation
            if (ref_PM25 >= AQI_INDIA[x]._25um_low && ref_PM25 <= AQI_INDIA[x]._25um_high)
            {
                stat_25um = x;
            }

            if (ref_PM10>= AQI_INDIA[x]._10um_low && ref_PM10 <= AQI_INDIA[x]._10um_high)
            {
                stat_10um = x;
            }
        }

        if (stat_25um == stat_10um || stat_25um > stat_10um )
        {
            // use the 2.5um to calculate the AQI
            aqi = (AQI_INDIA[stat_25um]._val_high - AQI_INDIA[stat_25um]._val_low) / (AQI_INDIA[stat_25um]._25um_high -AQI_INDIA[stat_25um]._25um_low);
            aqi = aqi * (ref_PM25 - AQI_INDIA[stat_25um]._25um_low) + AQI_INDIA[stat_25um]._val_low;

            strcpy(r->aqi_name, AQI_INDIA[stat_25um].cat_name);
            r->aqi_bnd_high = AQI_INDIA[stat_25um]._val_high;
            r->aqi_bnd_low = AQI_INDIA[stat_25um]._val_low;
            r->aqi_bnd = stat_25um+1;
            r->aqi_indicator = PM25;
        }

        else
        {
            // use the 10um situation as it is worse than 2.5um
            aqi = (AQI_INDIA[stat_10um]._val_high - AQI_INDIA[stat_10um]._val_low) / (AQI_INDIA[stat_10um]._10um_high -AQI_INDIA[stat_10um]._10um_low);
            aqi = aqi * (ref_PM10 - AQI_INDIA[stat_10um]._10um_low) + AQI_INDIA[stat_10um]._val_low;

            strcpy(r->aqi_name, AQI_INDIA[stat_10um].cat_name);
            r->aqi_bnd_high = AQI_INDIA[stat_10um]._val_high;
            r->aqi_bnd_low = AQI_INDIA[stat_10um]._val_low;
            r->aqi_bnd = stat_10um+1;
            r->aqi_indicator = PM10;
        }

        r->aqi_index = aqi;
    }
    else if (r->nv._region == CANADA1)
    {
        // use PM2.5 AQI
        for (x = 0; x < sizeof(CAN_AQI) / sizeof(struct AQI_area); x++)
        {
            // find the situation
            if (ref_PM25 >= CAN_AQI[x]._25um_low && ref_PM25 <= CAN_AQI[x]._25um_high)
            {
                stat_25um = x;
            }
        }

        r->aqi_index = (CAN_AQI[stat_25um]._val_high - CAN_AQI[stat_25um]._val_low) / (CAN_AQI[stat_25um]._25um_high -CAN_AQI[stat_25um]._25um_low);
        r->aqi_index = r->aqi_index * (ref_PM25 - CAN_AQI[stat_25um]._25um_low) + CAN_AQI[stat_25um]._val_low;

        strcpy(r->aqi_name, CAN_AQI[stat_25um].cat_name);
        r->aqi_bnd_high = CAN_AQI[stat_25um]._val_high;
        r->aqi_bnd_low = CAN_AQI[stat_25um]._val_low;
        r->aqi_bnd = stat_25um+1;
        r->aqi_indicator = PM25;
    }

    else if (r->nv._region == CANADA2)
    {
        // use PM2.5 AQHI
        for (x = 0; x < sizeof(CAN_AQHI) / sizeof(struct AQI_area); x++)
        {
            // find the situation
            if (ref_PM25 >= CAN_AQHI[x]._25um_low && ref_PM25 <= CAN_AQHI[x]._25um_high)
            {
                stat_25um = x;
            }
        }

        strcpy(r->aqi_name, CAN_AQHI[stat_25um].cat_name);
        r->aqi_bnd_high = CAN_AQHI[stat_25um]._val_high;
        r->aqi_bnd_low = CAN_AQHI[stat_25um]._val_low;
        r->aqi_bnd = stat_25um+1;
        r->aqi_index = stat_25um+1;     // no band boundery values defined for AQHI
        r->aqi_indicator = PM25;
    }
    else   //... other areas to add here and in aqi_area.h
        return(false);

    // set reference value used
    if (r->aqi_indicator == PM25) r->aqi_pmvalue = ref_PM25;
    else r->aqi_pmvalue = ref_PM10;

    // successfully completed
    return(true);
}

/**
 * @brief : get the raw information of the stored values
 *
 * @param r : place to store the AQI-NVRAM content
 */
void AQI::GetNv(struct AQI_NVRAM *r)
{
    if (r == NULL) return;

    // read novram values
    read_nvram(r);
}
