#ifndef DIALOG_IDX_REVIEW_H
#define DIALOG_IDX_REVIEW_H

#include "dialog_idx_review_base.h"

class PCB_BASE_FRAME;
class BOARD;
class KICAD_IDX;

/**
 * Derived dialog implementing a functional GAL preview panel.
 */
class DIALOG_IDX_REVIEW : public DIALOG__IDX_REVIEW_BASE
{
public:
    DIALOG_IDX_REVIEW( PCB_BASE_FRAME* aParent, KICAD_IDX* aIDX );
    ~DIALOG_IDX_REVIEW() override;

    bool TransferDataToWindow();
    std::vector<std::shared_ptr<KICAD_IDX_CHANGE>> GetAccepted();
    std::vector<std::shared_ptr<KICAD_IDX_CHANGE>> GetRejected();


protected:
    void OnGridCellClick( wxGridEvent& event ) override;
    void OnAcceptAll( wxCommandEvent& event ) override;
	void OnRejectAll( wxCommandEvent& event ) override;

private:
    void prepareCanvas();
    void refreshPreview();
    void OnResize( wxSizeEvent& event );
    void OnCancel( wxCommandEvent& event );
    void AddChange( KICAD_IDX_CHANGE& aChange);


private:
    PCB_BASE_FRAME* m_parent;
    BOARD*          m_board;
    BOARD*          m_dummyBoard;
    KICAD_IDX*      m_idx;

    std::vector<std::vector<BOARD_ITEM*>> m_objects;
    std::vector<std::pair<bool, std::shared_ptr<KICAD_IDX_CHANGE>>> m_changes;;


};

#endif // DIALOG_IDX_REVIEW_H
