#include "TextService.h"
#include "EditSession.h"

#include <assert.h>

using namespace Ime;

TextService::TextService(void):
	threadMgr_(NULL),
	clientId_(TF_CLIENTID_NULL),
	threadMgrEventSinkCookie_(TF_INVALID_COOKIE),
	textEditSinkCookie_(TF_INVALID_COOKIE),
	compositionSinkCookie_(TF_INVALID_COOKIE),
	composition_(NULL),
	refCount_(1) {
}

TextService::~TextService(void) {
}

// public methods

bool TextService::isComposing() {
	return (composition_ != NULL);
}

void TextService::startComposition(EditSession* session) {
	ITfContext* context = session->context();
	if(context) {
		HRESULT sessionResult;
		StartCompositionEditSession* session = new StartCompositionEditSession(this, context);
		context->RequestEditSession(clientId_, session, TF_ES_SYNC|TF_ES_READWRITE, &sessionResult);
		session->Release();
		context->Release();
	}
}

void TextService::endComposition(EditSession* session) {
	ITfContext* context = session->context();
	if(context) {
		HRESULT sessionResult;
		EndCompositionEditSession* session = new EndCompositionEditSession(this, context);
		context->RequestEditSession(clientId_, session, TF_ES_SYNC|TF_ES_READWRITE, &sessionResult);
		session->Release();
		context->Release();
	}
}

void TextService::replaceSelectedText(EditSession* session, const wchar_t* str, int len) {
	ITfContext* context = session->context();
	if(context) {
		TF_SELECTION selection;
		ULONG selectionNum;
		if(context->GetSelection(session->editCookie(), TF_DEFAULT_SELECTION, 1, &selection, &selectionNum) == S_OK) {
			if(selectionNum > 0 && selection.range) {
				selection.range->SetText(session->editCookie(), 0, str, len);
				selection.range->Release();
			}
		}
	}
}


// virtual
void TextService::onActivate() {
}

// virtual
void TextService::onDeactivate() {
}

// virtual
bool TextService::filterKeyDown(long key) {
	return false;
}

// virtual
bool TextService::onKeyDown(long key, EditSession* session) {
	return false;
}

// virtual
bool TextService::filterKeyUp(long key) {
	return false;
}

// virtual
bool TextService::onKeyUp(long key, EditSession* session) {
	return false;
}

// virtual
void TextService::onFocus() {
}


// COM stuff

// IUnknown
STDMETHODIMP TextService::QueryInterface(REFIID riid, void **ppvObj) {
    if (ppvObj == NULL)
        return E_INVALIDARG;
	if(IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfTextInputProcessor))
		*ppvObj = (ITfTextInputProcessor*)this;
	else if(IsEqualIID(riid, IID_ITfDisplayAttributeProvider))
		*ppvObj = (ITfDisplayAttributeProvider*)this;
	else if(IsEqualIID(riid, IID_ITfThreadMgrEventSink))
		*ppvObj = (ITfThreadMgrEventSink*)this;
	else if(IsEqualIID(riid, IID_ITfTextEditSink))
		*ppvObj = (ITfTextEditSink*)this;
	else if(IsEqualIID(riid, IID_ITfKeyEventSink))
		*ppvObj = (ITfKeyEventSink*)this;
	else if(IsEqualIID(riid, IID_ITfCompositionSink))
		*ppvObj = (ITfCompositionSink*)this;
	else
		*ppvObj = NULL;

	if(*ppvObj) {
		AddRef();
		return S_OK;
	}
	return E_NOINTERFACE;
}

// IUnknown implementation
STDMETHODIMP_(ULONG) TextService::AddRef(void) {
	return ++refCount_;
}

STDMETHODIMP_(ULONG) TextService::Release(void) {
	assert(refCount_ > 0);
	--refCount_;
	if(0 == refCount_)
		delete this;
	return refCount_;
}

// ITfTextInputProcessor
STDMETHODIMP TextService::Activate(ITfThreadMgr *pThreadMgr, TfClientId tfClientId) {
	// store tsf manager & client id
	threadMgr_ = pThreadMgr;
	threadMgr_->AddRef();
	clientId_ = tfClientId;

	// advice event sinks (set up event listeners)
	
	// ITfThreadMgrEventSink
	ITfSource *source;
	if(threadMgr_->QueryInterface(IID_ITfSource, (void **)&source) != S_OK)
		goto OnError;
	source->AdviseSink(IID_ITfThreadMgrEventSink, (ITfThreadMgrEventSink *)this, &threadMgrEventSinkCookie_);
	source->Release();

	// ITfTextEditSink,

	// ITfKeyEventSink
	ITfKeystrokeMgr *keystrokeMgr;
	if (threadMgr_->QueryInterface(IID_ITfKeystrokeMgr, (void **)&keystrokeMgr) != S_OK)
		goto OnError;
	keystrokeMgr->AdviseKeyEventSink(clientId_, (ITfKeyEventSink*)this, TRUE);
	keystrokeMgr->Release();

	// ITfCompositionSink

	onActivate();

	return S_OK;

OnError:
	Deactivate();
	return E_FAIL;
}

STDMETHODIMP TextService::Deactivate() {

	onDeactivate();

	// unadvice event sinks

	// ITfThreadMgrEventSink
	ITfSource *source;
	if(threadMgr_->QueryInterface(IID_ITfSource, (void **)&source) == S_OK) {
		source->UnadviseSink(threadMgrEventSinkCookie_);
		source->Release();
		threadMgrEventSinkCookie_ = TF_INVALID_COOKIE;
	}

	// ITfTextEditSink,

	// ITfKeyEventSink
	ITfKeystrokeMgr *keystrokeMgr;
	if (threadMgr_->QueryInterface(IID_ITfKeystrokeMgr, (void **)&keystrokeMgr) == S_OK) {
		keystrokeMgr->UnadviseKeyEventSink(clientId_);
		keystrokeMgr->Release();
	}

	// ITfCompositionSink

	if(threadMgr_) {
		threadMgr_->Release();
		threadMgr_ = NULL;
	}
	clientId_ = TF_CLIENTID_NULL;
	return S_OK;
}


// ITfThreadMgrEventSink
STDMETHODIMP TextService::OnInitDocumentMgr(ITfDocumentMgr *pDocMgr) {
	return S_OK;
}

STDMETHODIMP TextService::OnUninitDocumentMgr(ITfDocumentMgr *pDocMgr) {
	return S_OK;
}

STDMETHODIMP TextService::OnSetFocus(ITfDocumentMgr *pDocMgrFocus, ITfDocumentMgr *pDocMgrPrevFocus) {
	return S_OK;
}

STDMETHODIMP TextService::OnPushContext(ITfContext *pContext) {
	return S_OK;
}

STDMETHODIMP TextService::OnPopContext(ITfContext *pContext) {
	return S_OK;
}


// ITfTextEditSink
STDMETHODIMP TextService::OnEndEdit(ITfContext *pContext, TfEditCookie ecReadOnly, ITfEditRecord *pEditRecord) {
	return S_OK;
}


// ITfKeyEventSink
STDMETHODIMP TextService::OnSetFocus(BOOL fForeground) {
	onFocus();
	return S_OK;
}

STDMETHODIMP TextService::OnTestKeyDown(ITfContext *pContext, WPARAM wParam, LPARAM lParam, BOOL *pfEaten) {
	*pfEaten = (BOOL)filterKeyDown((long)wParam);
	return S_OK;
}

STDMETHODIMP TextService::OnKeyDown(ITfContext *pContext, WPARAM wParam, LPARAM lParam, BOOL *pfEaten) {
	HRESULT sessionResult;
	KeyEditSession* session = new KeyEditSession(this, pContext, true, wParam, lParam);
	pContext->RequestEditSession(clientId_, session, TF_ES_SYNC|TF_ES_READWRITE, &sessionResult);
	*pfEaten = session->result_; // tell TSF if we handled the key
	session->Release();
	return S_OK;
}

STDMETHODIMP TextService::OnTestKeyUp(ITfContext *pContext, WPARAM wParam, LPARAM lParam, BOOL *pfEaten) {
	*pfEaten = (BOOL)filterKeyUp((long)wParam);
	return S_OK;
}

STDMETHODIMP TextService::OnKeyUp(ITfContext *pContext, WPARAM wParam, LPARAM lParam, BOOL *pfEaten) {
	HRESULT sessionResult;
	KeyEditSession* session = new KeyEditSession(this, pContext, false, wParam, lParam);
	pContext->RequestEditSession(clientId_, session, TF_ES_SYNC|TF_ES_READWRITE, &sessionResult);
	*pfEaten = session->result_; // tell TSF if we handled the key
	session->Release();
	return S_OK;

	return S_OK;
}

STDMETHODIMP TextService::OnPreservedKey(ITfContext *pContext, REFGUID rguid, BOOL *pfEaten) {
	return S_OK;
}


// ITfCompositionSink
STDMETHODIMP TextService::OnCompositionTerminated(TfEditCookie ecWrite, ITfComposition *pComposition) {
	return S_OK;
}


// ITfDisplayAttributeProvider
STDMETHODIMP TextService::EnumDisplayAttributeInfo(IEnumTfDisplayAttributeInfo **ppEnum) {
	return S_OK;
}

STDMETHODIMP TextService::GetDisplayAttributeInfo(REFGUID guidInfo, ITfDisplayAttributeInfo **ppInfo) {
	return S_OK;
}

// edit session handling
STDMETHODIMP TextService::KeyEditSession::DoEditSession(TfEditCookie ec) {
	EditSession::DoEditSession(ec);
	return textService_->doKeyEditSession(ec, this);
}

// edit session handling
STDMETHODIMP TextService::StartCompositionEditSession::DoEditSession(TfEditCookie ec) {
	EditSession::DoEditSession(ec);
	return textService_->doStartCompositionEditSession(ec, this);
}

// edit session handling
STDMETHODIMP TextService::EndCompositionEditSession::DoEditSession(TfEditCookie ec) {
	EditSession::DoEditSession(ec);
	return textService_->doEndCompositionEditSession(ec, this);
}

// callback from edit session of key events
HRESULT TextService::doKeyEditSession(TfEditCookie cookie, KeyEditSession* session) {
	// TODO: perform key handling
	if(session->isDown_)
		session->result_ = onKeyDown((long)session->wParam_, session);
	else
		session->result_ = onKeyUp((long)session->wParam_, session);
	return S_OK;
}

// callback from edit session for starting composition
HRESULT TextService::doStartCompositionEditSession(TfEditCookie cookie, StartCompositionEditSession* session) {
	ITfContext* context = session->context();
	ITfContextComposition* contextComposition;
	if(context->QueryInterface(IID_ITfContextComposition, (void**)&contextComposition) == S_OK) {
		// get current insertion point in the current context
		ITfRange* range = NULL;
		ITfInsertAtSelection* insertAtSelection;
		if(context->QueryInterface(IID_ITfInsertAtSelection, (void **)&insertAtSelection) == S_OK) {
			// get current selection range & insertion position (query only, did not insert any text)
			insertAtSelection->InsertTextAtSelection(cookie, TF_IAS_QUERYONLY, NULL, 0, &range);
			insertAtSelection->Release();
		}

		if(range) {
			if(contextComposition->StartComposition(cookie, range, (ITfCompositionSink*)this, &composition_) == S_OK) {
				// according to the TSF sample provided by M$, we need to reset current
				// selection here. (maybe the range is altered by StartComposition()?
				// So mysterious. TSF is absolutely overly-engineered!
				TF_SELECTION selection;
				selection.range = range;
				selection.style.ase = TF_AE_NONE;
				selection.style.fInterimChar = FALSE;
				context->SetSelection(cookie, 1, &selection);
				// we did not release composition_ object. we store it for use later
			}
			range->Release();
		}
		contextComposition->Release();
	}
	return S_OK;
}

// callback from edit session for ending composition
HRESULT TextService::doEndCompositionEditSession(TfEditCookie cookie, EndCompositionEditSession* session) {
	if(composition_) {
		composition_->EndComposition(cookie);
		composition_->Release();
		composition_ = NULL;
	}
	return S_OK;
}

ITfContext* TextService::currentContext() {
	ITfContext* context = NULL;
	ITfDocumentMgr  *docMgr;
	if(threadMgr_->GetFocus(&docMgr) == S_OK) {
		docMgr->GetTop(&context);
		docMgr->Release();
	}
	return context;
}
