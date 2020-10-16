/**
 * Qir Quality Index (AQI) header file
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
 * This header file describes the limits to check per region
 * *
 * more info : https://en.wikipedia.org/wiki/Air_quality_index
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

#ifndef AQI_AREAH
#define AQI_AREAH

enum region_t {
    NOREGION = 0,
    USA = 1,                // EPA
    EUROPE1 = 2,            // CAQI PM10
    EUROPE2 = 3,            // CAQI PM2.5
    UK = 4,                 // COMEAP
    INDIA = 5,              // National Air Quality Index (AQI)
    CANADA1 = 6,            // AQI
    CANADA2 = 7             // AHQI
};

struct AQI_region {
    region_t  ind;
    char      name[20];
};

// must align with region_t entries
static struct AQI_region regions[8] = {
    {NOREGION, "No region"},
    {USA,      "USA"},
    {EUROPE1,  "EUROPE1 (PM10)"},
    {EUROPE2,  "EUROPE2 (PM2.5)"},
    {UK,       "UK"},
    {INDIA,    "INDIA"},
    {CANADA1,  "CANADA1 (AQI)"},
    {CANADA2,  "CANADA2 (AHQI)"}
};

/* To define the region AQI-data to compare the measured values against */
struct AQI_area {
    uint8_t ind;                // offset number
    char    cat_name[25];       // name for band
    float   _val_low;           // AQI low value for band
    float   _val_high;          // AQI high value for band
    float   _25um_low;          // 2.5um band low value to compare measured value
    float   _25um_high;         // 2.5um band high value to compare measured value
    float   _10um_low;          // 10um band low value to compare measured value
    float   _10um_high;         // 10um band high value to compare measured value
};

/* USA
 * https://en.wikipedia.org/wiki/Air_quality_index#cite_note-aqi_basic-11
 *
 * Daily numbers
 *
 * The worst pollutant determines the index.
 */
static struct AQI_area AQI_USA[7] = {

// ind, cat cat_name         val_low  val_high  25um_low   25um_high   10um_low   10um_high
   {1, "Good",                0.0,     50.0,     0.0,       12.0,        0.0,          54.0 },
   {2, "Moderate",            51.0,    100.0,    12.1,      35.4,        55.0,         154.0 },
   {3, "Unhealty for groups", 101.0,   150.0,    35.5,      55.4,        155.0,        254.0 },
   {4, "Unhealty",            151.0,   200.0,    55.5,      150.4,       255.0,        354.0 },
   {5, "Very Unhealty",       201.0,   300.0,    155.5,     250.4,       355.0,        424.0 },
   {6, "Hazardous",           301.0,   500.0,    250.5,     350.4,       425.0,        504.0 },
   {7, "STRONG Hazardous",    301.0,   500.0,    350.5,     500.4,       505.0,        604.0 }
};


/* EU: http://ec.europa.eu/environment/air/quality/standards.htm#_blank
 *
 * The yearly numbers are very hard to get and these definitions are as good as useless for
 * monitoring as you only can review after each year. This has not been implemented
 *
 * Fine particles (PM2.5)  25 µg/m3***  1 year
 * PM10                    50 µg/m3     24 hours     max time exceeding 35  per year
 *                         40 µg/m3     1 year
 *
 * https://www.airqualitynow.eu/download/CITEAIR-Comparing_Urban_Air_Quality_across_Borders.pdf
 * https://en.wikipedia.org/wiki/Air_quality_index#cite_note-CAQI_definition-34
 *
 * CiteairII stated that having an air quality index that would be easy to present to the
 * general public was a major motivation, leaving aside the more complex question of a
 * health-based index, which would require, for example, effects of combined levels
 * of different pollutants.
 *
 * The main aim of the CAQI was to have an index that would encourage wide comparison
 * across the EU, without replacing local indices. CiteairII stated that the "main goal
 * of the CAQI is not to warn people for possible adverse health effects of poor air
 * quality but to attract their attention to urban air pollution and its main source
 * (traffic) and help them decrease their exposure.
 *
 * The introduction of limit values for PM 2.5 made it necessary to accommodate that pollutant
 * in the index. Though PM 2.5 is an important pollutant it is not included as a core pollutant.
 * This is due to the way the EU PM 2.5 monitoring requirements are formulated and to the fact
 * that implementation has started only recently. It should not be seen as a sign that it is less
 * important than the mandatory pollutants nor that it hardly determines the overall index.
 *
 * While there is a difference between traffic and city background, it has no impact on the
 * level of PM2.5 and PM10. The full definition takes also NO2, CO, O3, SO2 into account
 * The worst core pollutant determines the index.
 *
 * if region EUROPE1 is set the PM10 value will be used, for EUROPE2 the PM2.5 value
 *
 */

static struct AQI_area AQI_CAQI_hrly[5] = {

// cat cat_name          val_low  val_high  25um_low   25um_high   10um_low   10um_high
   {1, "Very low",        0.0,     25.0,    0.0,       15.0,        0.0,       25.0 },
   {2, "Low",             25.0,    50.0,    15.0,      30.0,        25.0,      50.0 },
   {3, "Medium",          50.0,    75.0,    30.0,      55.0,        50.0,      90.0 },
   {4, "High",            75.0,    100.0,   55.0,      110.0,       90.0,      180.0},
   {5, "Very High",       100.0,   1000.0,  110.0,     11000.0,     180.0,     1800.0}        // xxx_high the value means anything > LOW
};

static struct AQI_area AQI_CAQI_daily[5] = {

// ind cat_name          val_low  val_high  25um_low   25um_high   10um_low   10um_high
   {1, "Very low",        0.0,     25.0,    0.0,       10.0,        0.0,       15.0 },
   {2, "Low",             25.0,    50.0,    10.0,      20.0,        15.0,      30.0 },
   {3, "Medium",          50.0,    75.0,    20.0,      30.0,        30.0,      50.0 },
   {4, "High",            75.0,    100.0,   30.0,      60.0,        50.0,      100.0},
   {5, "Very High",       100.0,   1000.0,  60.0,      6000.0,      100.0,     1000.0}        // xxx_high the value means anything > LOW
};


/* UK
 *
 * https://en.wikipedia.org/wiki/Air_quality_index
 *
 * The most commonly used air quality index in the UK is the Daily Air Quality Index recommended
 * by the Committee on Medical Effects of Air Pollutants (COMEAP).[31] This index has ten points,
 * which are further grouped into 4 bands: low, moderate, high and very high. Each of the bands
 * comes with advice for at-risk groups and the general population.
 *
 * The worst pollutant determines the index. There is no seperate HIGH low value.
 */

 static struct AQI_area UK_daily[10] = {

// ind cat_name          val_low  val_high  25um_low   25um_high   10um_low   10um_high
   {1, "Low1",               0,      0,    0.0,       11.0,        0.0,       16.0 },
   {2, "Low2",               0,      0,    11.0,      23.0,        17.0,      33.0 },
   {3, "Low3",               0,      0,    23.0,      35.0,        33.0,      50.0 },
   {4, "Moderate1",          0,      0,    35.0,      41.0,        50.0,      58.0 },
   {5, "Moderate2",          0,      0,    41.0,      47.0,        58.0,      66.0 },
   {6, "Moderate3",          0,      0,    47.0,      53.0,        66.0,      75.0 },
   {7, "High1",              0,      0,    53.0,      58.0,        75.0,      83.0 },
   {8, "High2",              0,      0,    58.0,      64.0,        83.0,      91.0 },
   {9, "High3",              0,      0,    64.0,      70.0,        91.0,      100.0},
   {10,"Very High",          0,      0,    70.0,      7000.0,      100.0,     1000.0}        // xxx_high the value means anything > LOW
};


/* INDIA
 * https://en.wikipedia.org/wiki/Air_quality_index#cite_note-aqi_basic-11
 *
 * Daily numbers
 * There are six AQI categories, namely Good, Satisfactory, Moderately polluted, Poor,
 * Very Poor, and Severe. The proposed AQI will consider eight pollutants (PM10, PM2.5,
 * NO2, SO2, CO, O3, NH3, and Pb) for which short-term (up to 24-hourly averaging period)
 * National Ambient Air Quality Standards are prescribed.[24] Based on the measured ambient
 * concentrations, corresponding standards and likely health impact, a sub-index is
 * calculated for each of these pollutants. The worst sub-index reflects overall AQI.
 */
static struct AQI_area AQI_INDIA[6] = {

// ind, cat cat_name         val_low  val_high  25um_low   25um_high   10um_low   10um_high
   {1, "Good",                 0.0,     50.0,     0.0,      30.0,        0.0,         50.0  },
   {2, "Satisfactory",        50.0,    100.0,    30.0,      60.0,       50.0,        100.0  },
   {3, "Moderately polluted", 100.0,   250.0,    60.0,      90.0,       100.0,       250.0  },
   {4, "Poor",                250.0,   350.0,    90.0,      120.0,      250.0,       350.0  },
   {5, "Very poor ",          350.0,   430.0,    120.0,     250.0,      350.0,       430.0  },
   {6, "Severe",              430.0,   500.0,    250.0,     1000.0,     430.0,       1000.0 }      // xxx_high the value means anything > LOW
};


/* CANADA
 *
 * Has multiple approaches that differ per province:  AQHI, AQI and AlBerta
 *
 * Some look at AQHI is a more leading indicator, as takes forecast and looks to health impact.
 * AQI is more driven by the PM2.5 and AQHI is more driven by NO2.
 *
 * Alberta has modified AQHI reporting to better suit the needs of the Province.
 * Because of Alberta's energy based economy other pollutants are also considered when reporting the AQHI.
 * (WE DID NOT INCLUDE ALBERTA)
 *
 * The AQI and AQHI use a number of pollutants. We only use on the PM2.5 values to determine the index and as
 * such it is not a 100% implementation of the index, but good-enough... for now.
 *
 * Canada AQI
 * It uses less pollutants and has a different scale. it is based on other AQI (like US)
 *
 * A document that describes AQI and AQHI.
 * https://www.publichealthontario.ca/-/media/documents/air-quality-health-index.pdf?la=en
 *
 *
 * AQHI
 * https://en.wikipedia.org/wiki/Air_Quality_Health_Index_(Canada)
 *
 * First, the average concentration of the 3 substances (O3, NO2, PM2.5) is calculated at each station
 * within a community for the 3 preceding hours. We use the average PM2.5 value.
 *
 *
 * Second, the 3 hour "community average" for each parameter is calculated from the 3 hour substance
 * averages at the available stations. If no stations are available for a parameter, that parameter
 * is set to "Not Available". This part of the process results in 3 community parameter averages.
 * We use "Not Available" and the averaged PM2.5 value.
 *
 * Third, if all three community parameter averages are available, a community AQHI is calculated.
 * The formula is:
 *  *
 * AQHI = (1000/10.4) * ((e^(0.000537*O3) - 1) + (e^(0.000871*NO2) - 1) + (e^(0.000487*PM2.5) -1))
 * The result is then rounded to the nearest positive integer; a calculation less than 0.5 is rounded up to 1.
 *
 * Simplifying the above AQHI formula using Taylor series approximation as follows:
 *
 * AQHI     ~ 10/10.4*100*{(1+0.000871*NO2)-1 +(1+0.000537*O3)-1 + (1+0.000487*PM2.5)–1}
 *          = 0.084 * NO2 + 0.052 * O3 + 0.047 * PM2.5
 *
 * The impact of PM25 is 25.68%
 *
 * This relation is seen in document https://www.publichealthontario.ca/-/media/documents/air-quality-health-index.pdf?la=en,
 * where also a relation is seen of 0.6 between AQHI and PM2.5.
 *
 * WE WILL USE : AQHI = (1000/10.4) * ((e^(0.000487*PM2.5 * 0.6) -1) / 0.2568)
 * This scale has been pre-calculated in CAN_AQHI[]
 *
 * Looking at the relationship in the aforementioned document, between the bands of AQI and AQHI, this is very close:
 *
 * very good/ good  <=> low quality = 83%  (document 97%)
 * moderate  <=>   moderate qualtiy = 83%  (document 79%)
 * poor/high <=>  high qaulity = 90% (document 90%)
 *
 *
 * The good & bad: we get high pollution at the nearly the same time....  no matter what scale is used.
 */

static struct AQI_area CAN_AQI[5] = {
// ind cat_name          val_low  val_high  25um_low   25um_high   10um_low   10um_high
   {1, "very good",      0,          15,      0.0,       12.0,        0,       0 },
   {2, "good",           15,         31,      12.0,      22.0,        0,       0 },
   {4, "Moderate",       31,         49,      22.0,      45.0,        0,       0 },
   {4, "poor",           49,         99,      45.0,      90.0,        0,       0 },
   {5, "Very poor",      99,         1000,    90.0,      1000.0,      0,       0 }        // xxx_high the value means anything >LOW
};

static struct AQI_area CAN_AQHI[10] = {
// ind cat_name          val_low  val_high  25um_low   25um_high   10um_low   10um_high
   {1, "Low-1",             0,         0,      12.0,       9.0,        0,       0 },
   {2, "Low-2",             0,         0,      9.0,       18.0,        0,       0 },
   {4, "Low-3",             0,         0,      18.0,      27.0,        0,       0 },
   {4, "Moderate-1",        0,         0,      27.0,      36.0,        0,       0 },
   {5, "Moderate-2",        0,         0,      36.0,      45.0,        0,       0 },
   {6, "Moderate-3",        0,         0,      45.0,      54.0,        0,       0 },
   {7, "High-1",            0,         0,      54.0,      63.0,        0,       0 },
   {8, "High-2",            0,         0,      63.0,      72.0,        0,       0 },
   {9, "High-3",            0,         0,      72.0,      81.0,        0,       0 },
   {10, "Very High",        0,         0,      81.0,      8100.0,      0,       0 }        // xxx_high the value means anything >LOW
};

#endif
