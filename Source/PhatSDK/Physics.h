
#pragma once

#include "SmartArray.h"
#include "HashData.h"

class SmartBox;
//class CObjectMaint;
// class CPhysicsObj;

class CPhysics
{
public:
    static SmartArray<std::weak_ptr<CPhysicsObj> > static_animating_objects;

    static void AddStaticAnimatingObject(std::shared_ptr<CPhysicsObj> pObject);
    static void RemoveStaticAnimatingObject(std::shared_ptr<CPhysicsObj> pObject);

    CPhysics(SmartBox *_SmartBox);
    ~CPhysics();

    void SetPlayer(std::shared_ptr<CPhysicsObj> Player);
    void UseTime();
    void UpdateTexVelocity(float FrameTime);

    //CObjectMaint *    m_ObjMaint;                  // 0x00
    SmartBox *        m_SmartBox;                  // 0x04
    std::weak_ptr<CPhysicsObj>     m_Player;                   // 0x08
	//TODO mwnciau
    //HashBaseIter<std::weak_ptr<CPhysicsObj> >* m_Iter;    // 0x0C
    DWORD            m_10;                          // 0x10
};

class PhysicsTimer
{
public:
    static double curr_time;
};

class PhysicsGlobals
{
public:
    static double floor_z;
    static double ceiling_z;
    static double gravity;
};
