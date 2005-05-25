/****************************************************************************
** ui.h extension file, included from the uic-generated form implementation.
**
** If you want to add, delete, or rename functions or slots, use
** Qt Designer to update this file, preserving your code.
**
** You should not define a constructor or destructor in this file.
** Instead, write your code in functions called init() and destroy().
** These will automatically be called by the form's constructor and
** destructor.
*****************************************************************************/

void VoIPLinkManagement::init()
{
parameters->setTitle(listVoiplink->text(0));
}

void VoIPLinkManagement::changeParamSlot()
{
    int index = listVoiplink->currentItem();
    parameters->setTitle(listVoiplink->text(index));
}


void VoIPLinkManagement::moveUpItemSlot()
{     
    int i = listVoiplink->currentItem();
        if (i > 0) {
   QString temp = listVoiplink->text(i - 1);
                listVoiplink->changeItem( listVoiplink->text(i), i - 1);
   listVoiplink ->changeItem(temp, i);
                listVoiplink   ->setCurrentItem(i - 1);
        }
}


void VoIPLinkManagement::moveDownItemSlot()
{
   int i = listVoiplink->currentItem();
        if (i < (int)listVoiplink->count() - 1) { 
            QString temp = listVoiplink->text(i + 1);
            listVoiplink->changeItem( listVoiplink->text(i), i + 1);
            listVoiplink ->changeItem(temp, i);
            listVoiplink   ->setCurrentItem(i + 1);
        }
}


void VoIPLinkManagement::addVoIPLinkSlot()
{
    listVoiplink->insertItem(QString("TOTO"));
}


void VoIPLinkManagement::removeVoIPLinkSlot()
{
    listVoiplink->removeItem(listVoiplink->currentItem());
}
