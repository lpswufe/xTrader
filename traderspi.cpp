#include "StdAfx.h"
#include "traderspi.h"
#include "xTrader.h"
#include "xTraderDlg.h"
#include "NoticeDlg.h"
#include "BfTransfer.h"

#pragma warning(disable :4996)

extern HANDLE g_hEvent;
extern CWnd* g_pCWnd;

//网络故障恢复正常后 自动重连
void CtpTraderSpi::OnFrontConnected()
{
	CXTraderApp* pApp = (CXTraderApp*)AfxGetApp();
	if (g_pCWnd)
	{
		m_bReconnect = TRUE;
		CXTraderDlg* pDlg = (CXTraderDlg*)g_pCWnd;
		//pDlg->SetStatusTxt( _T("TD√"),1);

		ReqUserLogin(pApp->m_sBROKER_ID,pApp->m_sINVESTOR_ID,pApp->m_sPASSWORD);
		DWORD dwRet = WaitForSingleObject(g_hEvent,WAIT_MS);
		if (dwRet==WAIT_OBJECT_0)
		{
			ResetEvent(g_hEvent);

			pDlg->m_pCliInfo->FrtID = m_ifrontId;
			pDlg->m_pCliInfo->SesID = m_isessionId;
	
			pDlg->SetTipTxt(_T("交易在线"),IDS_TRADE_TIPS);
			pDlg->SetPaneTxtColor(1,RED);

			SYSTEMTIME curTime;
			::GetLocalTime(&curTime);
			CString	szT;

			szT.Format(_T("%02d:%02d:%02d CTP重登录成功"), curTime.wHour, curTime.wMinute, curTime.wSecond);
			pDlg->SetStatusTxt(szT,2);
		
		}
		else
			return;
	}
	
	else
	{
		ReqUserLogin(pApp->m_sBROKER_ID,pApp->m_sINVESTOR_ID,pApp->m_sPASSWORD);
	}

  //if (g_bOnce)SetEvent(g_hEvent);
}

CtpTraderSpi::~CtpTraderSpi()
{
	ClrAllVecs();
}


void CtpTraderSpi::ReqUserLogin(TThostFtdcBrokerIDType vAppId,TThostFtdcUserIDType vUserId,TThostFtdcPasswordType vPasswd)
{
	CThostFtdcReqUserLoginField req;
	ZeroMemory(&req, sizeof(req));
	strcpy(req.BrokerID, vAppId); strcpy(BROKER_ID, vAppId); 
	strcpy(req.UserID, vUserId);  strcpy(INVEST_ID, vUserId); 
	strcpy(req.Password, vPasswd);
	strcpy(req.UserProductInfo,PROD_INFO);

	int iRet = pUserApi->ReqUserLogin(&req, ++m_iRequestID);
}

#define TIME_NULL "--:--:--"

void CtpTraderSpi::OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin,
		CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if ( pRspInfo){memcpy(&m_RspMsg,pRspInfo,sizeof(CThostFtdcRspInfoField));}

	if (!IsErrorRspInfo(pRspInfo) && pRspUserLogin )
	{  
    // 保存会话参数	
		m_ifrontId = pRspUserLogin->FrontID;
		m_isessionId = pRspUserLogin->SessionID;
		strcpy(m_sTdday,pRspUserLogin->TradingDay);

		int nextOrderRef = atoi(pRspUserLogin->MaxOrderRef);
		sprintf(m_sOrdRef, "%d", ++nextOrderRef);

		SYSTEMTIME curTime;
		::GetLocalTime(&curTime);
		CTime tc(curTime);
		int i=0;
		int iHour[4],iMin[4],iSec[4];
		if (!strcmp(pRspUserLogin->DCETime,TIME_NULL) || !strcmp(pRspUserLogin->SHFETime,TIME_NULL))
		{
			
			for (i=0;i<4;i++)
			{
				iHour[i]=curTime.wHour;
				iMin[i]=curTime.wMinute;
				iSec[i]=curTime.wSecond;
			}
		}
		else
		{
			sscanf(pRspUserLogin->SHFETime, "%d:%d:%d", &iHour[0], &iMin[0], &iSec[0]);
			sscanf(pRspUserLogin->DCETime, "%d:%d:%d", &iHour[1], &iMin[1], &iSec[1]);
			sscanf(pRspUserLogin->CZCETime, "%d:%d:%d", &iHour[2], &iMin[2], &iSec[2]);
			sscanf(pRspUserLogin->FFEXTime, "%d:%d:%d", &iHour[3], &iMin[3], &iSec[3]);
		}
	
		CTime t[4];
		for (i=0;i<4;i++)
		{
			t[i] = CTime(curTime.wYear,curTime.wMonth,curTime.wDay,iHour[i],iMin[i],iSec[i]);
			m_tsEXnLocal[i] = t[i]-tc;
		}

		//sprintf(m_sTmBegin,"%02d:%02d:%02d.%03d",curTime.wHour,curTime.wMinute,curTime.wSecond,curTime.wMilliseconds);
	    
	}

  if(bIsLast) SetEvent(g_hEvent);
}

const char* CtpTraderSpi::GetTradingDay()
{
	return m_sTdday;
}

void CtpTraderSpi::ReqUserLogout()
{
	CThostFtdcUserLogoutField req;
	ZeroMemory(&req, sizeof(req));
	strcpy(req.BrokerID, BROKER_ID);
	strcpy(req.UserID, INVEST_ID);
	pUserApi->ReqUserLogout(&req, ++m_iRequestID);
}

///登出请求响应
void CtpTraderSpi::OnRspUserLogout(CThostFtdcUserLogoutField *pUserLogout, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if( !IsErrorRspInfo(pRspInfo) && pUserLogout)
	{

	}
  if(bIsLast) SetEvent(g_hEvent);
}

void CtpTraderSpi::ReqSettlementInfoConfirm()
{
	CThostFtdcSettlementInfoConfirmField req;
	ZeroMemory(&req, sizeof(req));
	strcpy(req.BrokerID, BROKER_ID);
	strcpy(req.InvestorID, INVEST_ID);
	pUserApi->ReqSettlementInfoConfirm(&req, ++m_iRequestID);
}

void CtpTraderSpi::OnRspSettlementInfoConfirm(
        CThostFtdcSettlementInfoConfirmField  *pSettlementInfoConfirm, 
        CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{	
	if( !IsErrorRspInfo(pRspInfo) && pSettlementInfoConfirm)
	{
 
	}
  if(bIsLast) SetEvent(g_hEvent);
}

void CtpTraderSpi::ReqQryNotice()
{
	CThostFtdcQryNoticeField req;
	ZeroMemory(&req, sizeof(req));
	strcpy(req.BrokerID, BROKER_ID);
	pUserApi->ReqQryNotice(&req, ++m_iRequestID);
}

void CtpTraderSpi::OnRspQryNotice(CThostFtdcNoticeField *pNotice, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (!IsErrorRspInfo(pRspInfo) &&  pNotice)
	{
	}
	
	if(bIsLast) SetEvent(g_hEvent);
}

void CtpTraderSpi::ReqQryTdNotice()
{
	CThostFtdcQryTradingNoticeField req;
	ZeroMemory(&req, sizeof(req));
	strcpy(req.BrokerID, BROKER_ID);
	strcpy(req.InvestorID, INVEST_ID);
	pUserApi->ReqQryTradingNotice(&req, ++m_iRequestID);
}

void CtpTraderSpi::OnRspQryTradingNotice(CThostFtdcTradingNoticeField *pTradingNotice, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (!IsErrorRspInfo(pRspInfo) &&  pTradingNotice)
	{
	}
	
	if(bIsLast) SetEvent(g_hEvent);
}

///交易通知
void CtpTraderSpi::OnRtnTradingNotice(CThostFtdcTradingNoticeInfoField *pTradingNoticeInfo)
{

}

void CtpTraderSpi::ReqQrySettlementInfoConfirm()
{
	CThostFtdcQrySettlementInfoConfirmField req;
	ZeroMemory(&req, sizeof(req));
	strcpy(req.BrokerID, BROKER_ID);
	strcpy(req.InvestorID, INVEST_ID);

	pUserApi->ReqQrySettlementInfoConfirm(&req,++m_iRequestID);
}

void CtpTraderSpi::OnRspQrySettlementInfoConfirm(CThostFtdcSettlementInfoConfirmField *pSettlementInfoConfirm, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (!IsErrorRspInfo(pRspInfo) &&  pSettlementInfoConfirm)
	{
		//
	}
	
	if(bIsLast) SetEvent(g_hEvent);
}

void CtpTraderSpi::ReqQrySettlementInfo(TThostFtdcDateType TradingDay)
{
	CThostFtdcQrySettlementInfoField req;
	ZeroMemory(&req, sizeof(req));

	strcpy(req.BrokerID, BROKER_ID);
	strcpy(req.InvestorID, INVEST_ID);
	strcpy(req.TradingDay,TradingDay);

	pUserApi->ReqQrySettlementInfo(&req,++m_iRequestID);
}

void CtpTraderSpi::OnRspQrySettlementInfo(CThostFtdcSettlementInfoField *pSettlementInfo, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (!IsErrorRspInfo(pRspInfo) &&  pSettlementInfo)
	{
		CThostFtdcSettlementInfoField* pSi = new CThostFtdcSettlementInfoField();
		memcpy(pSi,pSettlementInfo,sizeof(CThostFtdcSettlementInfoField));
		m_StmiVec.push_back(pSi);
	}
	if(bIsLast) 
	{ 
		SetEvent(g_hEvent);
		SendNotifyMessage(HWND_BROADCAST,WM_QRYSMI_MSG,0,0);		
	}
}

void CtpTraderSpi::ReqQryInst(TThostFtdcInstrumentIDType instId)
{
	CThostFtdcQryInstrumentField req;
	ZeroMemory(&req, sizeof(req));
	if (instId != NULL)
	{ strcpy(req.InstrumentID, instId); }
	
	pUserApi->ReqQryInstrument(&req, ++m_iRequestID);
}

void CtpTraderSpi::OnRspQryInstrument(CThostFtdcInstrumentField *pInstrument, 
         CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{ 
	if ( !IsErrorRspInfo(pRspInfo) &&  pInstrument)
	{
		PINSINFEX pInsInf = new INSINFEX;
		ZeroMemory(pInsInf,sizeof(INSINFEX));
		memcpy(pInsInf,  pInstrument, sizeof(INSTINFO));
		m_InsinfVec.push_back(pInsInf);
	}
  if(bIsLast) SetEvent(g_hEvent);
}

void CtpTraderSpi::ReqQryTdAcc()
{
	CThostFtdcQryTradingAccountField req;
	ZeroMemory(&req, sizeof(req));
	strcpy(req.BrokerID, BROKER_ID);
	strcpy(req.InvestorID, INVEST_ID);
	pUserApi->ReqQryTradingAccount(&req, ++m_iRequestID);
}

void CtpTraderSpi::OnRspQryTradingAccount(
    CThostFtdcTradingAccountField *pTradingAccount, 
   CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{ 
	if (!IsErrorRspInfo(pRspInfo) &&  pTradingAccount)
	{
		 CThostFtdcTradingAccountField* pAcc = new CThostFtdcTradingAccountField();
		 memcpy(pAcc,pTradingAccount,sizeof(CThostFtdcTradingAccountField));
		 m_TdAcc = *pAcc ;

		if (g_pCWnd)
		{
			SendNotifyMessage(g_pCWnd->m_hWnd,WM_QRYACC_MSG,0,(LPARAM)pAcc);
		}
		
	}
  if(bIsLast) SetEvent(g_hEvent);
}

//INSTRUMENT_ID设成部分字段,例如IF10,就能查出所有IF10打头的头寸
void CtpTraderSpi::ReqQryInvPos(TThostFtdcInstrumentIDType instId)
{
	CThostFtdcQryInvestorPositionField req;
	ZeroMemory(&req, sizeof(req));
	strcpy(req.BrokerID, BROKER_ID);
	strcpy(req.InvestorID, INVEST_ID);
	if (instId!=NULL)
	{strcpy(req.InstrumentID, instId);}		
	pUserApi->ReqQryInvestorPosition(&req, ++m_iRequestID);
}

void CtpTraderSpi::OnRspQryInvestorPosition(
    CThostFtdcInvestorPositionField *pInvestorPosition, 
    CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{ 
  if( !IsErrorRspInfo(pRspInfo) &&  pInvestorPosition )
  {
	 CThostFtdcInvestorPositionField* pInvPos = new CThostFtdcInvestorPositionField();
	  memcpy(pInvPos,  pInvestorPosition, sizeof(CThostFtdcInvestorPositionField));
		m_InvPosVec.push_back(pInvPos);
  }
  if(bIsLast) SetEvent(g_hEvent);	
}

//INSTRUMENT_ID设成部分字段,例如IF10,就能查出所有IF10打头的头寸
void CtpTraderSpi::ReqQryInvPosEx(TThostFtdcInstrumentIDType instId)
{
	CThostFtdcQryInvestorPositionDetailField req;
	ZeroMemory(&req, sizeof(req));
	strcpy(req.BrokerID, BROKER_ID);
	strcpy(req.InvestorID, INVEST_ID);
	if (instId!=NULL)
	{strcpy(req.InstrumentID, instId);}		
	pUserApi->ReqQryInvestorPositionDetail(&req, ++m_iRequestID);
	//cerr<<" 请求 | 发送持仓查询..."<<((ret == 0)?"成功":"失败")<<endl;
}

void CtpTraderSpi::OnRspQryInvestorPositionDetail(CThostFtdcInvestorPositionDetailField *pInvestorPositionDetail, 
									CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if( !IsErrorRspInfo(pRspInfo) &&  pInvestorPositionDetail)
	{

	}
	 if(bIsLast) SetEvent(g_hEvent);
}

void CtpTraderSpi::ReqQryInvPosCombEx(TThostFtdcInstrumentIDType instId)
{
	CThostFtdcQryInvestorPositionCombineDetailField req;
	ZeroMemory(&req, sizeof(req));
	strcpy(req.BrokerID, BROKER_ID);
	strcpy(req.InvestorID, INVEST_ID);
	if (instId!=NULL)
	{strcpy(req.CombInstrumentID, instId);}		
	pUserApi->ReqQryInvestorPositionCombineDetail(&req, ++m_iRequestID);
}

void CtpTraderSpi::OnRspQryInvestorPositionCombineDetail(CThostFtdcInvestorPositionCombineDetailField *pInvestorPositionDetail, 
												  CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if( !IsErrorRspInfo(pRspInfo) &&  pInvestorPositionDetail)
	{
		
	}
	if(bIsLast) SetEvent(g_hEvent);
}

void CtpTraderSpi::ReqOrdLimit(TThostFtdcInstrumentIDType instId,TThostFtdcDirectionType dir,
	 TThostFtdcCombOffsetFlagType kpp,TThostFtdcPriceType price,   TThostFtdcVolumeType vol)
{
	CThostFtdcInputOrderField req;
	ZeroMemory(&req, sizeof(req));	
	strcpy(req.BrokerID, BROKER_ID);
	strcpy(req.InvestorID, INVEST_ID); 
	strcpy(req.InstrumentID, instId); 	
	strcpy(req.OrderRef, m_sOrdRef);
	int nextOrderRef = atoi(m_sOrdRef);
	sprintf(m_sOrdRef, "%d", ++nextOrderRef);

	req.OrderPriceType = THOST_FTDC_OPT_LimitPrice;//价格类型=限价	
	req.Direction = MapDirection(dir,true);  //买卖方向	
	req.CombOffsetFlag[0] = MapOffset(kpp[0],true); //组合开平标志:开仓
	req.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;	  //组合投机套保标志
	req.LimitPrice = price;	//价格
	req.VolumeTotalOriginal = vol;	///数量	
	req.TimeCondition = THOST_FTDC_TC_GFD;  //有效期类型:当日有效
	req.VolumeCondition = THOST_FTDC_VC_AV; //成交量类型:任何数量
	req.MinVolume = 1;	//最小成交量:1	
	req.ContingentCondition = THOST_FTDC_CC_Immediately;  //触发条件:立即
	
	req.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;	//强平原因:非强平	
	req.IsAutoSuspend = 0;  //自动挂起标志:否	
	req.UserForceClose = 0;   //用户强评标志:否

	pUserApi->ReqOrderInsert(&req, ++m_iRequestID);

}

///市价单
void CtpTraderSpi::ReqOrdAny(TThostFtdcInstrumentIDType instId,TThostFtdcDirectionType dir, TThostFtdcCombOffsetFlagType kpp,TThostFtdcVolumeType vol)
{
	CThostFtdcInputOrderField req;
	ZeroMemory(&req, sizeof(req));	
	strcpy(req.BrokerID, BROKER_ID);
	strcpy(req.InvestorID, INVEST_ID); 
	strcpy(req.InstrumentID, instId); 	
	strcpy(req.OrderRef, m_sOrdRef);
	int nextOrderRef = atoi(m_sOrdRef);
	sprintf(m_sOrdRef, "%d", ++nextOrderRef);
	
	req.OrderPriceType = THOST_FTDC_OPT_AnyPrice;//市价
	req.Direction = MapDirection(dir,true);  //买卖方向	
	req.CombOffsetFlag[0] = MapOffset(kpp[0],true); //组合开平标志:开仓
	req.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;	  //组合投机套保标志
	//req.LimitPrice = price;	//价格
	req.VolumeTotalOriginal = vol;	///数量	
	req.TimeCondition = THOST_FTDC_TC_IOC;;  //立即有效
	req.VolumeCondition = THOST_FTDC_VC_AV; //成交量类型:任何数量
	req.MinVolume = 1;	//最小成交量:1	
	req.ContingentCondition = THOST_FTDC_CC_Immediately;  //触发条件:立即
	
	req.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;	//强平原因:非强平	
	req.IsAutoSuspend = 0;  //自动挂起标志:否	
	req.UserForceClose = 0;   //用户强评标志:否
	
	pUserApi->ReqOrderInsert(&req, ++m_iRequestID);	
}

void CtpTraderSpi::ReqOrdCondition(TThostFtdcInstrumentIDType instId,TThostFtdcDirectionType dir, TThostFtdcCombOffsetFlagType kpp,
        TThostFtdcPriceType price,TThostFtdcVolumeType vol,TThostFtdcPriceType stopPrice,TThostFtdcContingentConditionType conType)
{
	CThostFtdcInputOrderField req;
	ZeroMemory(&req, sizeof(req));	
	strcpy(req.BrokerID, BROKER_ID);  //经纪公司代码	
	strcpy(req.InvestorID, INVEST_ID); //投资者代码	
	strcpy(req.InstrumentID, instId); //合约代码	
	strcpy(req.OrderRef, m_sOrdRef);  //报单引用
	int nextOrderRef = atoi(m_sOrdRef);
	sprintf(m_sOrdRef, "%d", ++nextOrderRef);
	
	req.OrderPriceType = THOST_FTDC_OPT_LimitPrice;	
	req.Direction = MapDirection(dir,true);  //买卖方向	
	req.CombOffsetFlag[0] = MapOffset(kpp[0],true); //组合开平标志:开仓
	req.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
	req.LimitPrice = price;	//价格
	req.VolumeTotalOriginal = vol;	///数量	
	req.TimeCondition = THOST_FTDC_TC_GFD;  //当日有效
	req.VolumeCondition = THOST_FTDC_VC_AV; //成交量类型:任何数量
	req.MinVolume = 1;	//最小成交量:1	
	req.ContingentCondition = conType;  //触发条件
	
	req.StopPrice = stopPrice;  //止损价
	req.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;	//强平原因:非强平	
	req.IsAutoSuspend = 0;  //自动挂起标志:否	
	req.UserForceClose = 0;   //用户强评标志:否
	
	pUserApi->ReqOrderInsert(&req, ++m_iRequestID);
}

/*
FAK(Fill And Kill)指令就是将报单的有效期设为THOST_FTDC_TC_IOC,同时,成交量类型设为THOST_FTDC_VC_AV,即任意数量;
FOK(Fill Or Kill)指令是将报单的有效期类型设置为THOST_FTDC_TC_IOC,同时将成交量类型设置为THOST_FTDC_VC_CV,即全部数量.
此外,在FAK指令下,还可指定最小成交量,即在指定价位、满足最小成交数量以上成交,剩余订单被系统撤销,否则被系统全部撤销.此种状况下,
有效期类型设置为THOST_FTDC_TC_IOC,数量条件设为THOST_FTDC_VC_MV,同时设定MinVolume字段.
*/
void CtpTraderSpi::ReqOrdFAOK(TThostFtdcInstrumentIDType instId,TThostFtdcDirectionType dir,TThostFtdcCombOffsetFlagType kpp,
			TThostFtdcPriceType price,/*TThostFtdcVolumeType vol,*/TThostFtdcVolumeConditionType volconType,TThostFtdcVolumeType minVol)
{
	CThostFtdcInputOrderField req;
	ZeroMemory(&req, sizeof(req));	
	strcpy(req.BrokerID, BROKER_ID);
	strcpy(req.InvestorID, INVEST_ID); 
	strcpy(req.InstrumentID, instId); 	
	strcpy(req.OrderRef, m_sOrdRef);
	int nextOrderRef = atoi(m_sOrdRef);
	sprintf(m_sOrdRef, "%d", ++nextOrderRef);
	
	req.OrderPriceType = THOST_FTDC_OPT_LimitPrice; //限价
	req.Direction = MapDirection(dir,true);  //买卖方向	
	req.CombOffsetFlag[0] = MapOffset(kpp[0],true); //组合开平标志
	req.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;	//组合投机套保标志
	req.LimitPrice = price;	//价格
	//req.VolumeTotalOriginal = vol;	///数量	
	req.TimeCondition = THOST_FTDC_TC_IOC;  //立即
	req.VolumeCondition = volconType; //THOST_FTDC_VC_AV,THOST_FTDC_VC_MV;THOST_FTDC_VC_CV
	req.MinVolume = minVol;	//FAK在THOST_FTDC_VC_MV时候可指定MinVol,其它情况置0
	req.ContingentCondition = THOST_FTDC_CC_Immediately;  //触发条件:立即
	
	req.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;	//强平原因:非强平	
	req.IsAutoSuspend = 0;  //自动挂起标志:否	
	req.UserForceClose = 0;   //用户强评标志:否
	
	pUserApi->ReqOrderInsert(&req, ++m_iRequestID);
}

void CtpTraderSpi::OnRspOrderInsert(CThostFtdcInputOrderField *pInputOrder, 
          CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	CXTraderDlg* pDlg = (CXTraderDlg*)g_pCWnd;
	if( IsErrorRspInfo(pRspInfo) || (pInputOrder==NULL) )
	{
		TCHAR szErr[MAX_PATH];
		ansi2uni(CP_ACP,pRspInfo->ErrorMsg,szErr);

		CThostFtdcOrderField* ErrOrd = new CThostFtdcOrderField();
		ZeroMemory(ErrOrd, sizeof(CThostFtdcOrderField));
		
		getCurTime(ErrOrd->InsertTime);

		strcpy(ErrOrd->InstrumentID,pInputOrder->InstrumentID);
		ErrOrd->Direction = pInputOrder->Direction;
		strcpy(ErrOrd->CombOffsetFlag,pInputOrder->CombOffsetFlag);
		ErrOrd->OrderStatus = THOST_FTDC_OST_ErrOrd;
		ErrOrd->LimitPrice = pInputOrder->LimitPrice;
		ErrOrd->VolumeTotalOriginal = pInputOrder->VolumeTotalOriginal;

		ErrOrd->VolumeTotal = pInputOrder->VolumeTotalOriginal;
		ErrOrd->VolumeTraded = 0;

		m_orderVec.push_back(ErrOrd);
		pDlg->InsertLstOrder(ErrOrd,INSERT_OP,szErr);
	}
  if(bIsLast) SetEvent(g_hEvent);	
}

void CtpTraderSpi::ReqOrderCancel(TThostFtdcOrderSysIDType OrdSysID)
{
  bool found=false; UINT i=0;
  for(i=0;i<m_orderVec.size();i++){
    if(m_orderVec[i]->OrderSysID == OrdSysID){ found = true; break;}
  }
  if(!found)
  {
	  ////////报单已被成交或不存在///////////
	  return;
  } 

	CThostFtdcInputOrderActionField req;
	ZeroMemory(&req, sizeof(req));
	strcpy(req.BrokerID, BROKER_ID);   //经纪公司代码	
	strcpy(req.InvestorID, INVEST_ID); //投资者代码

	strcpy(req.ExchangeID, m_orderVec[i]->ExchangeID);
	strcpy(req.OrderSysID, OrdSysID);
	req.ActionFlag = THOST_FTDC_AF_Delete;  //操作标志 

	pUserApi->ReqOrderAction(&req, ++m_iRequestID);
}

void CtpTraderSpi::ReqOrderCancel(TThostFtdcInstrumentIDType instId,TThostFtdcOrderRefType OrdRef)
{
	// FrontID + SessionID + OrdRef + InstrumentID
	CThostFtdcInputOrderActionField req;
	ZeroMemory(&req, sizeof(req));
	strcpy(req.BrokerID, BROKER_ID);   //经纪公司代码	
	strcpy(req.InvestorID, INVEST_ID); //投资者代码
	strcpy(req.InstrumentID, instId); //合约代码
	strcpy(req.OrderRef, OrdRef); //报单引用	
	req.FrontID = m_ifrontId;           //前置编号	
	req.SessionID = m_isessionId;       //会话编号

	req.ActionFlag = THOST_FTDC_AF_Delete;  //操作标志 
	
	pUserApi->ReqOrderAction(&req, ++m_iRequestID);
}
void CtpTraderSpi::OnRspOrderAction(
      CThostFtdcInputOrderActionField *pInputOrderAction, 
      CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{	
  if (IsErrorRspInfo(pRspInfo) || (pInputOrderAction==NULL))
  {
	 if (g_pCWnd)
	{
	 //////////////////////////////////////////
		CXTraderDlg* pDlg = (CXTraderDlg*)g_pCWnd;

		pDlg->SetStatusTxt(_T("撤单失败!"),2);
	 }

  }
  if(bIsLast) SetEvent(g_hEvent);	
}

///报单回报
void CtpTraderSpi::OnRtnOrder(CThostFtdcOrderField *pOrder)
{	
  CThostFtdcOrderField* order = new CThostFtdcOrderField();
  memcpy(order,  pOrder, sizeof(CThostFtdcOrderField));
  bool founded=false;    UINT i=0;
  for(i=0; i<m_orderVec.size(); i++)
  {
    if(m_orderVec[i]->BrokerOrderSeq == order->BrokerOrderSeq) 
	{ founded=true;    break;}
  }
  //////修改已发出的报单状态
  if(founded) 
  {
	m_orderVec[i]= order; 

	if (g_pCWnd && !m_bReconnect)
	{
		//////////////////////////////////////////////////////////
		CXTraderDlg* pDlg = (CXTraderDlg*)g_pCWnd;

		pDlg->InsertLstOrder(order,i);
		///////////////////////////刷新挂单列表/////////////////////
		int nRet = pDlg->FindOrdInOnRoadVec(order->BrokerOrderSeq);
		//未加入挂单列表的
		if (nRet==-1)
		{
			//加入列表
			if ((order->OrderStatus == THOST_FTDC_OST_PartTradedQueueing) || 
				(order->OrderStatus == THOST_FTDC_OST_NoTradeQueueing))
			{
				pDlg->m_onRoadVec.push_back(order);
				pDlg->InsertLstOnRoad(order,INSERT_OP);
			}
		}
		else
		{
			//已经在列表的 如果完成 则删除
			if ((order->OrderStatus == THOST_FTDC_OST_AllTraded) ||
				(order->OrderStatus == THOST_FTDC_OST_Canceled))
			{
				//int iCount = pDlg->m_LstOnRoad.GetItemCount();
				pDlg->m_LstOnRoad.SetRedraw(FALSE);
				pDlg->m_LstOnRoad.DeleteItem(pDlg->m_onRoadVec.size()-1-nRet);
				pDlg->m_LstOnRoad.SetRedraw(TRUE);

				pDlg->m_onRoadVec.erase(pDlg->m_onRoadVec.begin()+nRet);

			}
			if((order->OrderStatus == THOST_FTDC_OST_PartTradedQueueing) || 
				(order->OrderStatus == THOST_FTDC_OST_NoTradeQueueing))
			{
				//存在的挂单 修改状态
				//int nRet2 = pDlg->FindOrdInOnRoadLst(order->OrderSysID);
				pDlg->InsertLstOnRoad(order,nRet);
			}

		} 
		/////////////////////////刷新持仓////////////////////////////////
		
		bool bExist = false;
		for(i=0;i<m_InvPosVec.size();i++)
		{
			if (!strcmp(order->InstrumentID,m_InvPosVec[i]->InstrumentID))
			{ bExist = true; break;}	
		}
		
		if (bExist)
		{
			
		}
		else
		{
			
		}
		/////////////////////////////////////////////////////////////////
	}

  } 
  ///////新增加委托单
  else 
  {
	m_orderVec.push_back(order);
	if (g_pCWnd)
	{
		//////////////////////////////////////////////////////////
		CXTraderDlg* pDlg = (CXTraderDlg*)g_pCWnd;
		pDlg->InsertLstOrder(order,INSERT_OP);
	}
  }

  SetEvent(g_hEvent);
}

///成交通知
void CtpTraderSpi::OnRtnTrade(CThostFtdcTradeField *pTrade)
{
  CThostFtdcTradeField* trade = new CThostFtdcTradeField();
  memcpy(trade,  pTrade, sizeof(CThostFtdcTradeField));
  bool founded=false;     unsigned int i=0;
  for(i=0; i<m_tradeVec.size(); i++){
    if(!strcmp(m_tradeVec[i]->TradeID,trade->TradeID)) {
      founded=true;   break;
    }
  }
  //////修改成交单状态
  if(founded) 
  {
	  m_tradeVec[i]= trade; 
	  
	  if (g_pCWnd && !m_bReconnect)
	  {
		  CXTraderDlg* pDlg = (CXTraderDlg*)g_pCWnd;
		  pDlg->InsertLstTrade(trade,i+1);  
	  } 
  }
  ///////新增加已成交单 
  else 
  {
	m_tradeVec.push_back(trade);
	if (g_pCWnd)
	{
		CXTraderDlg* pDlg = (CXTraderDlg*)g_pCWnd;		  
		pDlg->InsertLstTrade(trade,INSERT_OP);

		/////////////////////////刷新持仓////////////////////////////////
		
		bool bExist = false;
		for(i=0;i<m_InvPosVec.size();i++)
		{
			if (!strcmp(trade->InstrumentID,m_InvPosVec[i]->InstrumentID))
			{ bExist = true; break;}	
		}
		
		CThostFtdcInvestorPositionField* newInvPos = new CThostFtdcInvestorPositionField();
		ZeroMemory(newInvPos,sizeof(CThostFtdcInvestorPositionField));
		
		if (bExist)
		{
			if (trade->Direction == THOST_FTDC_D_Buy)
			{
				switch(trade->OffsetFlag)
				{
				case THOST_FTDC_OF_Open:
					break;
				case THOST_FTDC_OF_Close:
					break;
				case THOST_FTDC_OF_CloseToday:
					break;
				case THOST_FTDC_OF_CloseYesterday:
					break;
				case THOST_FTDC_OF_ForceOff:
					break;
				case THOST_FTDC_OF_LocalForceClose:
					break;			
				}
			}
			if (trade->Direction == THOST_FTDC_D_Sell)
			{
				switch(trade->OffsetFlag)
				{
				case THOST_FTDC_OF_Open:
					break;
				case THOST_FTDC_OF_Close:
					break;
				case THOST_FTDC_OF_CloseToday:
					break;
				case THOST_FTDC_OF_CloseYesterday:
					break;
				case THOST_FTDC_OF_ForceOff:
					break;
				case THOST_FTDC_OF_LocalForceClose:
					break;			
				}
			}
		}
		else
		{
			int iMul = pDlg->FindInstMul(trade->InstrumentID);
			strcpy(newInvPos->InstrumentID,trade->InstrumentID);
			strcpy(newInvPos->BrokerID,trade->BrokerID);
			strcpy(newInvPos->InvestorID,trade->InvestorID);
			newInvPos->PosiDirection = trade->Direction+2;
			newInvPos->HedgeFlag = trade->HedgeFlag;
			newInvPos->PositionDate = (strcmp(m_sTdday,trade->TradeDate)==0)?THOST_FTDC_PSD_Today:THOST_FTDC_PSD_History;
			newInvPos->Position = trade->Volume;
			newInvPos->OpenVolume = trade->Volume;
			newInvPos->OpenAmount = trade->Volume * trade->Price * iMul;
			newInvPos->PositionCost = trade->Volume * trade->Price * iMul;
			//newInvPos->UseMargin = newInvPos->PositionCost * pDlg->
			
			
		}
		/////////////////////////////////////////////////////////////////  
	}
 
  }
 
  SetEvent(g_hEvent);
}

void CtpTraderSpi::OnFrontDisconnected(int nReason)
{
	if (g_pCWnd)
	{
		CXTraderDlg* pDlg = (CXTraderDlg*)g_pCWnd;
		//pDlg->SetStatusTxt(_T("TD×"),1);
		pDlg->SetTipTxt(_T("交易断开"),IDS_TRADE_TIPS);
		pDlg->SetPaneTxtColor(1,BLUE);

		SYSTEMTIME curTime;
		::GetLocalTime(&curTime);
		CString	szT;
			
		
		szT.Format(_T("%02d:%02d:%02d CTP中断等待重连"), curTime.wHour, curTime.wMinute, curTime.wSecond);
		 pDlg->SetStatusTxt(szT,2);
		ShowErroTips(IDS_DISCONTIPS,IDS_STRTIPS);

	}
}
		
void CtpTraderSpi::OnHeartBeatWarning(int nTimeLapse)
{
	//控制台才用
}

///请求查询交易编码
void CtpTraderSpi::ReqQryTradingCode()
{

	CThostFtdcQryTradingCodeField req;
	ZeroMemory(&req, sizeof(req));

	req.ClientIDType = THOST_FTDC_CIDT_Speculation;

	strcpy(req.BrokerID, BROKER_ID);   //经纪公司代码	
	strcpy(req.InvestorID, INVEST_ID); //投资者代码

	pUserApi->ReqQryTradingCode(&req,++m_iRequestID);
}

void CtpTraderSpi::OnRspQryTradingCode(CThostFtdcTradingCodeField *pTradingCode, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if( !IsErrorRspInfo(pRspInfo) && pTradingCode )
	{
		CThostFtdcTradingCodeField* pTdCode = new CThostFtdcTradingCodeField();
		memcpy(pTdCode, pTradingCode, sizeof(CThostFtdcTradingCodeField));
		m_TdCodeVec.push_back(pTdCode);
	}
  if(bIsLast) SetEvent(g_hEvent);
}

///请求查询合约保证金率
void CtpTraderSpi::ReqQryInstMgr(TThostFtdcInstrumentIDType instId)
{
	CThostFtdcQryInstrumentMarginRateField req;

	ZeroMemory(&req, sizeof(req));
	strcpy(req.BrokerID, BROKER_ID);   //经纪公司代码	
	strcpy(req.InvestorID, INVEST_ID); //投资者代码
	if (instId!=NULL)
	{strcpy(req.InstrumentID, instId);}	
	req.HedgeFlag = THOST_FTDC_HF_Speculation;
	pUserApi->ReqQryInstrumentMarginRate(&req,++m_iRequestID);
}

void CtpTraderSpi::OnRspQryInstrumentMarginRate(CThostFtdcInstrumentMarginRateField *pInstrumentMarginRate, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if( !IsErrorRspInfo(pRspInfo) && pInstrumentMarginRate )
	{
		memcpy(&m_MargRateRev,  pInstrumentMarginRate, sizeof(CThostFtdcInstrumentMarginRateField));
		
	}
  if(bIsLast) SetEvent(g_hEvent);

}

///请求查询合约手续费率
void CtpTraderSpi::ReqQryInstFee(TThostFtdcInstrumentIDType instId)
{
	CThostFtdcQryInstrumentCommissionRateField req;
	
	ZeroMemory(&req, sizeof(req));
	strcpy(req.BrokerID, BROKER_ID);   //经纪公司代码	
	strcpy(req.InvestorID, INVEST_ID); //投资者代码
	if (instId!=NULL)
	{strcpy(req.InstrumentID, instId);}	
	pUserApi->ReqQryInstrumentCommissionRate(&req,++m_iRequestID);
}

void CtpTraderSpi::OnRspQryInstrumentCommissionRate(CThostFtdcInstrumentCommissionRateField *pInstrumentCommissionRate, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if( !IsErrorRspInfo(pRspInfo) && pInstrumentCommissionRate )
	{
		memcpy(&m_FeeRateRev,  pInstrumentCommissionRate, sizeof(CThostFtdcInstrumentCommissionRateField)); 
		
	}
  if(bIsLast) SetEvent(g_hEvent);

}

///请求查询投资者品种/跨品种保证金
void CtpTraderSpi::ReqQryInstGroupMargin(TThostFtdcInstrumentIDType instId)
{
	CThostFtdcQryInvestorProductGroupMarginField req;
	
	ZeroMemory(&req, sizeof(req));
	strcpy(req.BrokerID, BROKER_ID);   //经纪公司代码	
	strcpy(req.InvestorID, INVEST_ID); //投资者代码
	if (instId!=NULL)
	{strcpy(req.ProductGroupID, instId);}	

	pUserApi->ReqQryInvestorProductGroupMargin(&req,++m_iRequestID);
}

///请求查询投资者品种/跨品种保证金响应
void CtpTraderSpi::OnRspQryInvestorProductGroupMargin(CThostFtdcInvestorProductGroupMarginField *pInvestorProductGroupMargin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if( !IsErrorRspInfo(pRspInfo) && pInvestorProductGroupMargin )
	{
			
	}
	if(bIsLast) SetEvent(g_hEvent);	
}

///请求查询交易所保证金率
void CtpTraderSpi::ReqQryExhMgr(TThostFtdcInstrumentIDType instId)
{
	CThostFtdcQryExchangeMarginRateField req;
	
	ZeroMemory(&req, sizeof(req));
	strcpy(req.BrokerID, BROKER_ID);   //经纪公司代码	
	if (instId!=NULL)
	{strcpy(req.InstrumentID, instId);}	
	req.HedgeFlag = THOST_FTDC_HF_Speculation;

	pUserApi->ReqQryExchangeMarginRate(&req,++m_iRequestID);
}

///请求查询交易所保证金率响应
void CtpTraderSpi::OnRspQryExchangeMarginRate(CThostFtdcExchangeMarginRateField *pExchangeMarginRate, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if( !IsErrorRspInfo(pRspInfo) && pExchangeMarginRate )
	{
		
	}
	if(bIsLast) SetEvent(g_hEvent);	
}

///请求查询交易所调整保证金率
void CtpTraderSpi::ReqQryExhMgrAdjust(TThostFtdcInstrumentIDType instId)
{
	CThostFtdcQryExchangeMarginRateAdjustField req;

	ZeroMemory(&req, sizeof(req));
	strcpy(req.BrokerID, BROKER_ID);   //经纪公司代码	
	if (instId!=NULL)
	{strcpy(req.InstrumentID, instId);}	
	req.HedgeFlag = THOST_FTDC_HF_Speculation;

	pUserApi->ReqQryExchangeMarginRateAdjust(&req,++m_iRequestID);

}

///请求查询交易所调整保证金率响应
void CtpTraderSpi::OnRspQryExchangeMarginRateAdjust(CThostFtdcExchangeMarginRateAdjustField *pExchangeMarginRateAdjust, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if( !IsErrorRspInfo(pRspInfo) && pExchangeMarginRateAdjust )
	{
		
	}
	if(bIsLast) SetEvent(g_hEvent);
}

//////////////////请求查询用户资料/////////////
void CtpTraderSpi::ReqQryInvestor()
{
	CThostFtdcQryInvestorField req;

	ZeroMemory(&req, sizeof(req));
	strcpy(req.BrokerID, BROKER_ID);   //经纪公司代码	
	strcpy(req.InvestorID, INVEST_ID); //投资者代码

	pUserApi->ReqQryInvestor(&req, ++m_iRequestID);
}

void CtpTraderSpi::OnRspQryInvestor(CThostFtdcInvestorField *pInvestor, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if( !IsErrorRspInfo(pRspInfo) && pInvestor )
	{
		CThostFtdcInvestorField *pInv = new CThostFtdcInvestorField;
		memcpy(pInv,pInvestor,sizeof(CThostFtdcInvestorField));
		
		PostMessage(g_pCWnd->m_hWnd,WM_QRYUSER_MSG,0,(LPARAM)pInv);
	}
  if(bIsLast) SetEvent(g_hEvent);
}

void CtpTraderSpi::ReqUserPwdUpdate(TThostFtdcPasswordType szNewPass,TThostFtdcPasswordType szOldPass)
{
	CThostFtdcUserPasswordUpdateField req;

	ZeroMemory(&req, sizeof(req));
	strcpy(req.BrokerID, BROKER_ID);   //经纪公司代码	
	strcpy(req.UserID, INVEST_ID); //用户代码
	strcpy(req.NewPassword, szNewPass);    	
	strcpy(req.OldPassword,szOldPass);  

	pUserApi->ReqUserPasswordUpdate(&req,++m_iRequestID);
}

//资金账户密码
void CtpTraderSpi::ReqTdAccPwdUpdate(TThostFtdcPasswordType szNewPass,TThostFtdcPasswordType szOldPass)
{
	CThostFtdcTradingAccountPasswordUpdateField req;

	ZeroMemory(&req, sizeof(req));
	strcpy(req.BrokerID, BROKER_ID);   //经纪公司代码	
	strcpy(req.AccountID, INVEST_ID); //用户代码
	strcpy(req.NewPassword, szNewPass);    	
	strcpy(req.OldPassword,szOldPass);  

	pUserApi->ReqTradingAccountPasswordUpdate(&req,++m_iRequestID);
}

///用户口令更新请求响应
void CtpTraderSpi::OnRspUserPasswordUpdate(CThostFtdcUserPasswordUpdateField *pUserPasswordUpdate, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if( !IsErrorRspInfo(pRspInfo) && pUserPasswordUpdate )
	{
		ShowErroTips(IDS_MODPASSOK,IDS_STRTIPS);
	}
	else
	{
		ShowCbErrs(pRspInfo->ErrorMsg);
	}
	if(bIsLast) SetEvent(g_hEvent);
}

///资金账户口令更新请求响应
void CtpTraderSpi::OnRspTradingAccountPasswordUpdate(CThostFtdcTradingAccountPasswordUpdateField *pTradingAccountPasswordUpdate, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if( !IsErrorRspInfo(pRspInfo) && pTradingAccountPasswordUpdate )
	{
		ShowErroTips(IDS_MODPASSOK,IDS_STRTIPS);
	}
	else
	{
		ShowCbErrs(pRspInfo->ErrorMsg);
	}
  if(bIsLast) SetEvent(g_hEvent);
}

void CtpTraderSpi::ReqAuthenticate(TThostFtdcProductInfoType UserProdInf,TThostFtdcAuthCodeType	AuthCode)
{
	CThostFtdcReqAuthenticateField req;

	ZeroMemory(&req, sizeof(req));
	strcpy(req.BrokerID, BROKER_ID);   //经纪公司代码	
	strcpy(req.UserID, INVEST_ID); //用户代码
	strcpy(req.UserProductInfo, UserProdInf);    	
	strcpy(req.AuthCode,AuthCode);  

	pUserApi->ReqAuthenticate(&req,++m_iRequestID);
}

void CtpTraderSpi::OnRspAuthenticate(CThostFtdcRspAuthenticateField *pRspAuthenticateField, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if( !IsErrorRspInfo(pRspInfo) && pRspAuthenticateField )
	{
	}
  if(bIsLast) SetEvent(g_hEvent);
}

void CtpTraderSpi::ReqQryAccreg()
{
	CThostFtdcQryAccountregisterField req;
	ZeroMemory(&req, sizeof(req));

	strcpy(req.BrokerID, BROKER_ID);   //经纪公司代码	
	strcpy(req.AccountID, INVEST_ID); //用户代码

	pUserApi->ReqQryAccountregister(&req,++m_iRequestID);
}

void CtpTraderSpi::OnRspQryAccountregister(CThostFtdcAccountregisterField *pAccountregister, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if( !IsErrorRspInfo(pRspInfo) && pAccountregister)
	{
		CThostFtdcAccountregisterField* pAccReg = new CThostFtdcAccountregisterField();
		memcpy(pAccReg,  pAccountregister, sizeof(CThostFtdcAccountregisterField));
		m_AccRegVec.push_back(pAccReg);

	}
  if(bIsLast) SetEvent(g_hEvent);
}

void CtpTraderSpi::ReqQryTransBk(TThostFtdcBankIDType BankID,TThostFtdcBankBrchIDType BankBrchID)
{
	CThostFtdcQryTransferBankField req;
	ZeroMemory(&req, sizeof(req));
	if(BankID != NULL)
		strcpy(req.BankID,BankID);
	if(BankBrchID != NULL)
		strcpy(req.BankBrchID,BankBrchID);


	pUserApi->ReqQryTransferBank(&req,++m_iRequestID);
}

void CtpTraderSpi::OnRspQryTransferBank(CThostFtdcTransferBankField *pTransferBank, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if( !IsErrorRspInfo(pRspInfo) && pTransferBank)
	{
	}
  if(bIsLast) SetEvent(g_hEvent);
}

void CtpTraderSpi::ReqQryContractBk(TThostFtdcBankIDType BankID,TThostFtdcBankBrchIDType BankBrchID)
{
	CThostFtdcQryContractBankField req;

	ZeroMemory(&req, sizeof(req));
	strcpy(req.BrokerID, BROKER_ID);   //经纪公司代码	

	if(BankID != NULL)
		strcpy(req.BankID,BankID);
	if(BankBrchID != NULL)
		strcpy(req.BankBrchID,BankBrchID);

	pUserApi->ReqQryContractBank(&req,++m_iRequestID);
}

  void CtpTraderSpi::OnRspQryContractBank(CThostFtdcContractBankField *pContractBank, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
 {
	  if( !IsErrorRspInfo(pRspInfo) && pContractBank)
	  {
	  }
	if(bIsLast) SetEvent(g_hEvent);
 }

  //////////////////////////////////////////期货发起银行资金转期货请求///////////////////////////////////////
void CtpTraderSpi::ReqBk2FByF(TThostFtdcBankIDType BkID,TThostFtdcPasswordType BkPwd,
	 TThostFtdcPasswordType Pwd,TThostFtdcTradeAmountType TdAmt)
{
	CThostFtdcReqTransferField req;
	ZeroMemory(&req, sizeof(req));

	strcpy(req.BrokerID, BROKER_ID);   //经纪公司代码	 
	strcpy(req.AccountID, INVEST_ID); //用户代码
	strcpy(req.TradeCode,"202001");
	strcpy(req.BankBranchID,"0000");
	strcpy(req.CurrencyID,"RMB");
	strcpy(req.BankID,BkID);
	strcpy(req.BankPassWord,BkPwd);
	strcpy(req.Password,Pwd);
	req.TradeAmount=TdAmt;
	req.SecuPwdFlag = THOST_FTDC_BPWDF_BlankCheck;

	pUserApi->ReqFromBankToFutureByFuture(&req,++m_iRequestID);
}
 
 ///期货发起银行资金转期货应答
void CtpTraderSpi::OnRspFromBankToFutureByFuture(CThostFtdcReqTransferField *pReqTransfer, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if( IsErrorRspInfo(pRspInfo) || (pReqTransfer==NULL))
	{	
		ShowCbErrs(pRspInfo->ErrorMsg);
	}
	if(bIsLast) SetEvent(g_hEvent);	 
}
///期货发起银行资金转期货通知
void CtpTraderSpi::OnRtnFromBankToFutureByFuture(CThostFtdcRspTransferField *pRspTransfer)
{
	  if(g_pCWnd && (!m_bReconnect))
	  {
		  if(pRspTransfer->ErrorID!=0)
		  {
			  ShowCbErrs(pRspTransfer->ErrorMsg);
		  }
		  else
		  {
			  ShowErroTips(IDS_BFTRANS_OK,IDS_STRTIPS);
		  }
	  }

  SetEvent(g_hEvent);
}

///期货发起银行资金转期货错误回报
void CtpTraderSpi::OnErrRtnBankToFutureByFuture(CThostFtdcReqTransferField *pReqTransfer, CThostFtdcRspInfoField *pRspInfo)
{

}


///////////////////////////////////////////期货发起期货资金转银行请求///////////////////////////////////////////
void CtpTraderSpi::ReqF2BkByF(TThostFtdcBankIDType BkID,TThostFtdcPasswordType BkPwd,
	 TThostFtdcPasswordType Pwd,TThostFtdcTradeAmountType TdAmt)
 {
	 CThostFtdcReqTransferField req;
	 ZeroMemory(&req, sizeof(req));
	 
	 strcpy(req.BrokerID, BROKER_ID);   //经纪公司代码	 
	 strcpy(req.AccountID, INVEST_ID); //用户代码
	 strcpy(req.TradeCode,"202002");
	 strcpy(req.BankBranchID,"0000");
	 strcpy(req.CurrencyID,"RMB");
	 strcpy(req.BankID,BkID);
	 strcpy(req.BankPassWord,BkPwd);
	 strcpy(req.Password,Pwd);
	 req.TradeAmount=TdAmt;
	 req.SecuPwdFlag = THOST_FTDC_BPWDF_BlankCheck;
	 
	 pUserApi->ReqFromFutureToBankByFuture(&req,++m_iRequestID);
 }


 ///期货发起期货资金转银行应答
 void CtpTraderSpi::OnRspFromFutureToBankByFuture(CThostFtdcReqTransferField *pReqTransfer, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
 {
	 if( IsErrorRspInfo(pRspInfo) || (pReqTransfer==NULL))
	 {

		ShowCbErrs(pRspInfo->ErrorMsg);

	 }

	 if(bIsLast) SetEvent(g_hEvent);	
 }

 
///期货发起期货资金转银行通知
void CtpTraderSpi::OnRtnFromFutureToBankByFuture(CThostFtdcRspTransferField *pRspTransfer)
{
	  if(g_pCWnd&&(!m_bReconnect))
	  {
		  if(pRspTransfer->ErrorID!=0)
		  {
			  ShowCbErrs(pRspTransfer->ErrorMsg);
		  }
		  else
		  {
			  ShowErroTips(IDS_BFTRANS_OK,IDS_STRTIPS);
		  }
	  }
  
  SetEvent(g_hEvent);
}



///期货发起期货资金转银行错误回报
void CtpTraderSpi::OnErrRtnFutureToBankByFuture(CThostFtdcReqTransferField *pReqTransfer, CThostFtdcRspInfoField *pRspInfo)
{

}

 ///////////////////////////////////////////////////期货发起查询银行余额请求///////////////////////////////////////////////
void CtpTraderSpi::ReqQryBkAccMoneyByF(TThostFtdcBankIDType BkID,TThostFtdcPasswordType BkPwd,
	 TThostFtdcPasswordType Pwd)
{
	CThostFtdcReqQueryAccountField req;
	ZeroMemory(&req, sizeof(req));
	
	strcpy(req.BrokerID, BROKER_ID);   //经纪公司代码	 
	strcpy(req.AccountID, INVEST_ID); //用户代码
	strcpy(req.TradeCode,"204002");
	strcpy(req.BankBranchID,"0000");
	strcpy(req.CurrencyID,"RMB");
	strcpy(req.BankID,BkID);
	strcpy(req.BankPassWord,BkPwd);
	strcpy(req.Password,Pwd);

	req.SecuPwdFlag = THOST_FTDC_BPWDF_BlankCheck;
	pUserApi->ReqQueryBankAccountMoneyByFuture(&req,++m_iRequestID);
}

///期货发起查询银行余额应答
void CtpTraderSpi::OnRspQueryBankAccountMoneyByFuture(CThostFtdcReqQueryAccountField *pReqQueryAccount, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	
	if( IsErrorRspInfo(pRspInfo) || (pReqQueryAccount==NULL))
	{

		ShowCbErrs(pRspInfo->ErrorMsg);
	}
	if(bIsLast) SetEvent(g_hEvent);	
	
}

///期货发起查询银行余额通知
void CtpTraderSpi::OnRtnQueryBankBalanceByFuture(CThostFtdcNotifyQueryAccountField *pNotifyQueryAccount)
{
	if(pNotifyQueryAccount->ErrorID ==0)
	{
		if (g_pCWnd && (!m_bReconnect))
		{
			CThostFtdcNotifyQueryAccountField *pNotify = new CThostFtdcNotifyQueryAccountField();
			memcpy(pNotify,pNotifyQueryAccount,sizeof(CThostFtdcNotifyQueryAccountField));

			::PostMessage(g_pCWnd->m_hWnd,WM_QRYBKYE_MSG,0,(LPARAM)pNotify);
		}

	}

	SetEvent(g_hEvent);	
}


///期货发起查询银行余额错误回报
void CtpTraderSpi::OnErrRtnQueryBankBalanceByFuture(CThostFtdcReqQueryAccountField *pReqQueryAccount, CThostFtdcRspInfoField *pRspInfo)
{

}

///////////////////////////////////////查询转账流水////////////////////////////////////////////
/// "204005"
void CtpTraderSpi::ReqQryTfSerial(TThostFtdcBankIDType BkID)
{
	CThostFtdcQryTransferSerialField req;
	ZeroMemory(&req, sizeof(req));

	strcpy(req.BrokerID, BROKER_ID);   //经纪公司代码	 
	strcpy(req.AccountID, INVEST_ID); //用户代码

	strcpy(req.BankID,BkID);

	pUserApi->ReqQryTransferSerial(&req,++m_iRequestID);
}
///请求查询转帐流水响应
void CtpTraderSpi::OnRspQryTransferSerial(CThostFtdcTransferSerialField *pTransferSerial, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if( !IsErrorRspInfo(pRspInfo) && pTransferSerial)
	{
		//ShowCbErrs(pRspInfo->ErrorMsg);
		CThostFtdcTransferSerialField* bfTrans = new CThostFtdcTransferSerialField();
		ZeroMemory(bfTrans, sizeof(CThostFtdcTransferSerialField));
		memcpy(bfTrans,pTransferSerial,sizeof(CThostFtdcTransferSerialField));
		m_BfTransVec.push_back(bfTrans);
	}
	if(bIsLast) 
	{ 
		SetEvent(g_hEvent);

		SendNotifyMessage(HWND_BROADCAST,WM_QRYBFLOG_MSG,0,0);		
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
///系统运行时期货端手工发起冲正银行转期货请求，银行处理完毕后报盘发回的通知
void CtpTraderSpi::OnRtnRepealFromBankToFutureByFutureManual(CThostFtdcRspRepealField *pRspRepeal)
{
}


///系统运行时期货端手工发起冲正期货转银行请求，银行处理完毕后报盘发回的通知
void CtpTraderSpi::OnRtnRepealFromFutureToBankByFutureManual(CThostFtdcRspRepealField *pRspRepeal)
{
}


///系统运行时期货端手工发起冲正银行转期货错误回报
void CtpTraderSpi::OnErrRtnRepealBankToFutureByFutureManual(CThostFtdcReqRepealField *pReqRepeal, CThostFtdcRspInfoField *pRspInfo)
{

}

///系统运行时期货端手工发起冲正期货转银行错误回报
void CtpTraderSpi::OnErrRtnRepealFutureToBankByFutureManual(CThostFtdcReqRepealField *pReqRepeal, CThostFtdcRspInfoField *pRspInfo)
{
}


///期货发起冲正银行转期货请求，银行处理完毕后报盘发回的通知
void CtpTraderSpi::OnRtnRepealFromBankToFutureByFuture(CThostFtdcRspRepealField *pRspRepeal)
{
}


///期货发起冲正期货转银行请求，银行处理完毕后报盘发回的通知
void CtpTraderSpi::OnRtnRepealFromFutureToBankByFuture(CThostFtdcRspRepealField *pRspRepeal)
{
}



void CtpTraderSpi::ReqQryCFMMCTdAccKey()
{
	CThostFtdcQryCFMMCTradingAccountKeyField req;
	ZeroMemory(&req, sizeof(req));
	strcpy(req.BrokerID, BROKER_ID);   //经纪公司代码	 
	strcpy(req.InvestorID, INVEST_ID); //用户代码

	pUserApi->ReqQryCFMMCTradingAccountKey(&req,++m_iRequestID);
}

void CtpTraderSpi::OnRspQryCFMMCTradingAccountKey(CThostFtdcCFMMCTradingAccountKeyField *pCFMMCTradingAccountKey, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if( !IsErrorRspInfo(pRspInfo) && pCFMMCTradingAccountKey)
	{
		char strMsg[1000];
		sprintf(strMsg,CFMMC_TMPL,pCFMMCTradingAccountKey->ParticipantID,pCFMMCTradingAccountKey->AccountID,
			pCFMMCTradingAccountKey->KeyID,pCFMMCTradingAccountKey->CurrentKey);
		ShellExecuteA(NULL,"open",strMsg,NULL, NULL, SW_SHOW);

	}
	//if(bIsLast) SetEvent(g_hEvent);	
}

void CtpTraderSpi::ReqQryBkrTdParams()
{
	CThostFtdcQryBrokerTradingParamsField  req;
	ZeroMemory(&req, sizeof(req));
	strcpy(req.BrokerID,BROKER_ID);
	strcpy(req.InvestorID,INVEST_ID);

	pUserApi->ReqQryBrokerTradingParams(&req, ++m_iRequestID);
}

void CtpTraderSpi::OnRspQryBrokerTradingParams(CThostFtdcBrokerTradingParamsField *pBrokerTradingParams, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if( !IsErrorRspInfo(pRspInfo) && pBrokerTradingParams)
	{

	}
	if(bIsLast) SetEvent(g_hEvent);
}

void CtpTraderSpi::ReqQryBkrTdAlgos(TThostFtdcExchangeIDType ExhID,TThostFtdcInstrumentIDType instID)
{
	CThostFtdcQryBrokerTradingAlgosField  req;
	ZeroMemory(&req, sizeof(req));
	strcpy(req.BrokerID,BROKER_ID);
	if(ExhID != NULL)
		strcpy(req.ExchangeID, ExhID);
	if(instID != NULL)
		strcpy(req.InstrumentID, instID);
	
	pUserApi->ReqQryBrokerTradingAlgos(&req, ++m_iRequestID);
}

void CtpTraderSpi::OnRspQryBrokerTradingAlgos(CThostFtdcBrokerTradingAlgosField *pBrokerTradingAlgos, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if( !IsErrorRspInfo(pRspInfo) && pBrokerTradingAlgos)
	{
		
	}
	if(bIsLast) SetEvent(g_hEvent);
}

///预埋单录入请求
void CtpTraderSpi::ReqParkedOrderInsert(CThostFtdcParkedOrderField *ParkedOrder)
{
	pUserApi->ReqParkedOrderInsert(ParkedOrder,++m_iRequestID);
}
///预埋单录入请求响应
void CtpTraderSpi::OnRspParkedOrderInsert(CThostFtdcParkedOrderField *pParkedOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if( !IsErrorRspInfo(pRspInfo) && pParkedOrder)
	{
		
	}
	if(bIsLast) SetEvent(g_hEvent);
}

///预埋撤单录入请求
void CtpTraderSpi::ReqParkedOrderAction(CThostFtdcParkedOrderActionField *ParkedOrderAction)
{
	pUserApi->ReqParkedOrderAction(ParkedOrderAction,++m_iRequestID);
}
///预埋撤单录入请求响应
void CtpTraderSpi::OnRspParkedOrderAction(CThostFtdcParkedOrderActionField *pParkedOrderAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if( !IsErrorRspInfo(pRspInfo) && pParkedOrderAction)
	{
		
	}
	if(bIsLast) SetEvent(g_hEvent);
}

///请求查询预埋单
void CtpTraderSpi::ReqQryParkedOrder(TThostFtdcInstrumentIDType InstrumentID,TThostFtdcExchangeIDType ExchangeID)
{
	CThostFtdcQryParkedOrderField  req;
	ZeroMemory(&req, sizeof(req));
	strcpy(req.BrokerID,BROKER_ID);
	strcpy(req.InvestorID,INVEST_ID);
	if(InstrumentID != NULL)
		strcpy(req.InstrumentID,InstrumentID);
	if(ExchangeID != NULL)
		strcpy(req.ExchangeID,ExchangeID);
	pUserApi->ReqQryParkedOrder(&req, ++m_iRequestID);
}

///请求查询预埋单响应
void CtpTraderSpi::OnRspQryParkedOrder(CThostFtdcParkedOrderField *pParkedOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if( !IsErrorRspInfo(pRspInfo) && pParkedOrder)
	{
		
	}
	if(bIsLast) SetEvent(g_hEvent);
}

///请求撤销预埋
void CtpTraderSpi::ReqQryParkedOrderAction(TThostFtdcInstrumentIDType InstrumentID,TThostFtdcExchangeIDType ExchangeID)
{
	CThostFtdcQryParkedOrderActionField  req;
	ZeroMemory(&req, sizeof(req));
	strcpy(req.BrokerID,BROKER_ID);
	strcpy(req.InvestorID,INVEST_ID);
	if(InstrumentID != NULL)
		strcpy(req.InstrumentID,InstrumentID);
	if(ExchangeID != NULL)
		strcpy(req.ExchangeID,ExchangeID);
	pUserApi->ReqQryParkedOrderAction(&req, ++m_iRequestID);
}

///删除预埋撤单响应
void CtpTraderSpi::OnRspRemoveParkedOrderAction(CThostFtdcRemoveParkedOrderActionField *pRemoveParkedOrderAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) 
{
	if( !IsErrorRspInfo(pRspInfo) && pRemoveParkedOrderAction)
	{
		
	}
	if(bIsLast) SetEvent(g_hEvent);
}

///请求删除预埋单
void CtpTraderSpi::ReqRemoveParkedOrder(TThostFtdcParkedOrderIDType ParkedOrder_ID)
{
	CThostFtdcRemoveParkedOrderField  req;
	ZeroMemory(&req, sizeof(req));
	strcpy(req.BrokerID,BROKER_ID);
	strcpy(req.InvestorID,INVEST_ID);
	strcpy(req.ParkedOrderID,ParkedOrder_ID);
	pUserApi->ReqRemoveParkedOrder(&req, ++m_iRequestID);
}

///删除预埋单响应
void CtpTraderSpi::OnRspRemoveParkedOrder(CThostFtdcRemoveParkedOrderField *pRemoveParkedOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if( !IsErrorRspInfo(pRspInfo) && pRemoveParkedOrder)
	{
		
	}
	if(bIsLast) SetEvent(g_hEvent);
}

///请求删除预埋撤单
void CtpTraderSpi::ReqRemoveParkedOrderAction(TThostFtdcParkedOrderActionIDType ParkedOrderAction_ID)
{
	CThostFtdcRemoveParkedOrderActionField  req;
	ZeroMemory(&req, sizeof(req));
	strcpy(req.BrokerID,BROKER_ID);
	strcpy(req.InvestorID,INVEST_ID);
	strcpy(req.ParkedOrderActionID,ParkedOrderAction_ID);
	pUserApi->ReqRemoveParkedOrderAction(&req, ++m_iRequestID);
}

///请求删除预埋撤单响应
void CtpTraderSpi::OnRspQryParkedOrderAction(CThostFtdcParkedOrderActionField *pParkedOrderAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) 
{
	if( !IsErrorRspInfo(pRspInfo) && pParkedOrderAction)
	{
		
	}
	if(bIsLast) SetEvent(g_hEvent);
}

///提示条件单校验错误
void CtpTraderSpi::OnRtnErrorConditionalOrder(CThostFtdcErrorConditionalOrderField *pErrorConditionalOrder)
{
	
}

void CtpTraderSpi::OnRtnInstrumentStatus(CThostFtdcInstrumentStatusField *pInstrumentStatus)
{
	if(pInstrumentStatus && g_pCWnd)
	{
		CXTraderDlg* pDlg = (CXTraderDlg*)g_pCWnd;
		CString szStat,szMsg,szExh;
		TCHAR szTm[30];
		ansi2uni(CP_ACP,pInstrumentStatus->EnterTime,szTm);
		JgTdStatus(szStat,pInstrumentStatus->InstrumentStatus);
		szExh = JgExchage(pInstrumentStatus->ExchangeID);

		szMsg.Format(_T("%s %s:%s"),szTm,(LPCTSTR)szExh,(LPCTSTR)szStat);
		pDlg->SetStatusTxt(szMsg,2);
	}
}

void CtpTraderSpi::OnRspError(CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	IsErrorRspInfo(pRspInfo);
}

bool CtpTraderSpi::IsErrorRspInfo(CThostFtdcRspInfoField *pRspInfo)
{
	// 如果ErrorID != 0, 说明收到了错误的响应
	bool ret = ((pRspInfo) && (pRspInfo->ErrorID != 0));
	return ret;
}

void CtpTraderSpi::ShowCbErrs(TThostFtdcErrorMsgType ErrorMsg)
{
	TCHAR szMsg[MAX_PATH];
	ansi2uni(CP_ACP,ErrorMsg,szMsg);
	
	ShowErroTips(szMsg,MY_TIPS);
}

void CtpTraderSpi::ClrAllVecs()
{
	ClearVector(m_orderVec);
	ClearVector(m_tradeVec);
	ClearVector(m_InsinfVec);
	ClearVector(m_StmiVec);
	ClearVector(m_AccRegVec);
	ClearVector(m_TdCodeVec);
	ClearVector(m_InvPosVec);
	ClearVector(m_BfTransVec);

	ZeroMemory(&m_RspMsg,sizeof(CThostFtdcRspInfoField));
}