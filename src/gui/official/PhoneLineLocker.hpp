
#ifndef SFLPHONEGUI_PHONELINELOCKER_HPP
#define SFLPHONEGUI_PHONELINELOCKER_HPP

/**
 * This class is used as a Lock. It means
 * that it will lock a phone line on its
 * constructor, and unlock it in its 
 * destructor. This is generaly used to
 * be exception safe.
 */
class PhoneLineLocker
{
public:
  /**
   * Retreive the "line" PhoneLine and
   * locks it.
   */
  PhoneLineLocker(PhoneLine *line);

  /**
   * Unlock the currently locked PhoneLine.
   */
  ~PhoneLine();
}

#endif
