#ifndef VIEW_ITEM_H
#define VIEW_ITEM_H

//A simple interface for plasmoid items
class ViewItem
{
   public:
      virtual bool isConference() =0;
      virtual ~ViewItem() {}
};

#endif
