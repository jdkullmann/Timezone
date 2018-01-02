/*----------------------------------------------------------------------*
 * Arduino Timezone Library                                             *
 * Jack Christensen Mar 2012                                            *
 *                                                                      *
 * "Arduino Timezone Library" by Jack Christensen is licensed under     *
 * CC BY-SA 4.0, http://creativecommons.org/licenses/by-sa/4.0/         *
 *----------------------------------------------------------------------*/

#include "Timezone.h"

#ifdef __AVR__
    #include <avr/eeprom.h>
#endif

/*----------------------------------------------------------------------*
 * Create a Timezone object from the given time change rules.           *
 *----------------------------------------------------------------------*/
Timezone::Timezone(TimeChangeRule dstStart, TimeChangeRule stdStart)
{
    m_dst = dstStart;
    m_std = stdStart;
}

#ifdef __AVR__
/*----------------------------------------------------------------------*
 * Create a Timezone object from time change rules stored in EEPROM     *
 * at the given address.                                                *
 *----------------------------------------------------------------------*/
Timezone::Timezone(int address)
{
    readRules(address);
}
#endif

/*----------------------------------------------------------------------*
 * Convert the given UTC time to local time, standard or                *
 * daylight time, as appropriate.                                       *
 *----------------------------------------------------------------------*/
time_t Timezone::toLocal(time_t utc)
{
    // recalculate the time change points if needed
    if (year(utc) != year(m_dstUTC)) calcTimeChanges(year(utc));

    if (utcIsDST(utc))
        return utc + m_dst.offset * SECS_PER_MIN;
    else
        return utc + m_std.offset * SECS_PER_MIN;
}

/*----------------------------------------------------------------------*
 * Convert the given UTC time to local time, standard or                *
 * daylight time, as appropriate, and return a pointer to the time      *
 * change rule used to do the conversion. The caller must take care     *
 * not to alter this rule.                                              *
 *----------------------------------------------------------------------*/
time_t Timezone::toLocal(time_t utc, TimeChangeRule **tcr)
{
    // recalculate the time change points if needed
    if (year(utc) != year(m_dstUTC)) calcTimeChanges(year(utc));

    if (utcIsDST(utc)) {
        *tcr = &m_dst;
        return utc + m_dst.offset * SECS_PER_MIN;
    }
    else {
        *tcr = &m_std;
        return utc + m_std.offset * SECS_PER_MIN;
    }
}

/*----------------------------------------------------------------------*
 * Convert the given local time to UTC time.                            *
 *                                                                      *
 * WARNING:                                                             *
 * This function is provided for completeness, but should seldom be     *
 * needed and should be used sparingly and carefully.                   *
 *                                                                      *
 * Ambiguous situations occur after the Standard-to-DST and the         *
 * DST-to-Standard time transitions. When changing to DST, there is     *
 * one hour of local time that does not exist, since the clock moves    *
 * forward one hour. Similarly, when changing to standard time, there   *
 * is one hour of local times that occur twice since the clock moves    *
 * back one hour.                                                       *
 *                                                                      *
 * This function does not test whether it is passed an erroneous time   *
 * value during the Local -> DST transition that does not exist.        *
 * If passed such a time, an incorrect UTC time value will be returned. *
 *                                                                      *
 * If passed a local time value during the DST -> Local transition      *
 * that occurs twice, it will be treated as the earlier time, i.e.      *
 * the time that occurs before the transistion.                         *
 *                                                                      *
 * Calling this function with local times during a transition interval  *
 * should be avoided!                                                   *
 *----------------------------------------------------------------------*/
time_t Timezone::toUTC(time_t local)
{
    // recalculate the time change points if needed
    if (year(local) != year(m_dstLoc)) calcTimeChanges(year(local));

    if (locIsDST(local))
        return local - m_dst.offset * SECS_PER_MIN;
    else
        return local - m_std.offset * SECS_PER_MIN;
}

/*----------------------------------------------------------------------*
 * Determine whether the given UTC time_t is within the DST interval    *
 * or the Standard time interval.                                       *
 *----------------------------------------------------------------------*/
bool Timezone::utcIsDST(time_t utc)
{
    // recalculate the time change points if needed
    if (year(utc) != year(m_dstUTC)) calcTimeChanges(year(utc));

    if (m_stdUTC == m_dstUTC)       // daylight time not observed in this tz
        return false;
    else if (m_stdUTC > m_dstUTC)   // northern hemisphere
        return (utc >= m_dstUTC && utc < m_stdUTC);
    else                            // southern hemisphere
        return !(utc >= m_stdUTC && utc < m_dstUTC);
}

/*----------------------------------------------------------------------*
 * Determine whether the given Local time_t is within the DST interval  *
 * or the Standard time interval.                                       *
 *----------------------------------------------------------------------*/
bool Timezone::locIsDST(time_t local)
{
    // recalculate the time change points if needed
    if (year(local) != year(m_dstLoc)) calcTimeChanges(year(local));

    if (m_stdUTC == m_dstUTC)       // daylight time not observed in this tz
        return false;
    else if (m_stdLoc > m_dstLoc)   // northern hemisphere
        return (local >= m_dstLoc && local < m_stdLoc);
    else                            // southern hemisphere
        return !(local >= m_stdLoc && local < m_dstLoc);
}

/*----------------------------------------------------------------------*
 * Calculate the DST and standard time change points for the given      *
 * given year as local and UTC time_t values.                           *
 *----------------------------------------------------------------------*/
void Timezone::calcTimeChanges(int yr)
{
    m_dstLoc = toTime_t(m_dst, yr);
    m_stdLoc = toTime_t(m_std, yr);
    m_dstUTC = m_dstLoc - m_std.offset * SECS_PER_MIN;
    m_stdUTC = m_stdLoc - m_dst.offset * SECS_PER_MIN;
}

/*----------------------------------------------------------------------*
 * Convert the given DST change rule to a time_t value                  *
 * for the given year.                                                  *
 *----------------------------------------------------------------------*/
time_t Timezone::toTime_t(TimeChangeRule r, int yr)
{
    tmElements_t tm;
    time_t t;
    uint8_t m, w;            // temp copies of r.month and r.week

    m = r.month;
    w = r.week;
    if (w == 0) {            // Last week = 0
        if (++m > 12) {      // for "Last", go to the next month
            m = 1;
            yr++;
        }
        w = 1;               // and treat as first week of next month, subtract 7 days later
    }

    tm.Hour = r.hour;
    tm.Minute = 0;
    tm.Second = 0;
    tm.Day = 1;
    tm.Month = m;
    tm.Year = yr - 1970;
    t = makeTime(tm);        // first day of the month, or first day of next month for "Last" rules
    t += (7 * (w - 1) + (r.dow - weekday(t) + 7) % 7) * SECS_PER_DAY;
    if (r.week == 0) t -= 7 * SECS_PER_DAY;    // back up a week if this is a "Last" rule
    return t;
}

/*----------------------------------------------------------------------*
 * Read or update the daylight and standard time rules from RAM.        *
 *----------------------------------------------------------------------*/
void Timezone::readRules(TimeChangeRule dstStart, TimeChangeRule stdStart)
{
    m_dst = dstStart;
    m_std = stdStart;
    m_dstUTC = 0;   // force calcTimeChanges() at next conversion call
    m_stdUTC = 0;
    m_dstLoc = 0;
    m_stdLoc = 0;
}

#ifdef __AVR__
/*----------------------------------------------------------------------*
 * Read the daylight and standard time rules from EEPROM at             *
 * the given address.                                                   *
 *----------------------------------------------------------------------*/
void Timezone::readRules(int address)
{
    eeprom_read_block((void *) &m_dst, (void *) address, sizeof(m_dst));
    address += sizeof(m_dst);
    eeprom_read_block((void *) &m_std, (void *) address, sizeof(m_std));
    m_dstUTC = 0;   // force calcTimeChanges() at next conversion call
    m_stdUTC = 0;
    m_dstLoc = 0;
    m_stdLoc = 0;
}

/*----------------------------------------------------------------------*
 * Write the daylight and standard time rules to EEPROM at              *
 * the given address.                                                   *
 *----------------------------------------------------------------------*/
void Timezone::writeRules(int address)
{
    eeprom_write_block((void *) &m_dst, (void *) address, sizeof(m_dst));
    address += sizeof(m_dst);
    eeprom_write_block((void *) &m_std, (void *) address, sizeof(m_std));
}

#endif
