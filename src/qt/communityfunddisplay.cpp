#include "communityfunddisplay.h"
#include "ui_communityfunddisplay.h"
#include "main.h"
#include <QtWidgets/QDialogButtonBox>
#include "../txdb.h"
#include <iomanip>
#include <sstream>
#include <ctime>

#include "consensus/cfund.h"
#include "chain.h"
#include "guiutil.h"

#include "communityfunddisplaydetailed.h"
#include "communityfundpage.h"

CommunityFundDisplay::CommunityFundDisplay(QWidget *parent, CFund::CProposal proposal) :
    QWidget(parent),
    ui(new Ui::CommunityFundDisplay),
    proposal(proposal)
{
    ui->setupUi(this);

    QFont f_title("Sans Serif", 10.5, QFont::Bold);
    QFont f_label_title("Sans Serif", 10, QFont::Bold);
    QFont f_label("Sans Serif", 10, QFont::Normal);

    ui->title->setFont(f_title);
    ui->labelTitleDuration->setFont(f_label_title);
    ui->labelDuration->setFont(f_label);
    ui->labelTitleRequested->setFont(f_label_title);
    ui->labelRequested->setFont(f_label);
    ui->labelTitleStatus->setFont(f_label_title);
    ui->labelStatus->setFont(f_label);
    ui->pushButtonDetails->setFont(f_label);
    ui->buttonBoxVote->setFont(f_label);

    connect(ui->buttonBoxVote, SIGNAL(clicked( QAbstractButton*)), this, SLOT(click_buttonBoxVote(QAbstractButton*)));
    connect(ui->pushButtonDetails, SIGNAL(clicked()), this, SLOT(click_pushButtonDetails()));

    refresh();
}

void CommunityFundDisplay::refresh()
{
    //set labels from community fund
    ui->title->setText(QString::fromStdString(proposal.strDZeel));
    ui->labelStatus->setText(QString::fromStdString(proposal.GetState(pindexBestHeader->GetBlockTime())));
    stringstream n;
    n.imbue(std::locale(""));
    n << fixed << setprecision(8) << proposal.nAmount/100000000.0;
    string nav_amount = n.str();
    nav_amount.erase(nav_amount.find_last_not_of("0") + 1, std::string::npos );
    if(nav_amount.at(nav_amount.length()-1) == '.') {
        nav_amount = nav_amount.substr(0, nav_amount.size()-1);
    }
    nav_amount.append(" NAV");
    ui->labelRequested->setText(QString::fromStdString(nav_amount));

    uint64_t proptime = 0;
    if (mapBlockIndex.count(proposal.blockhash) > 0) {
        proptime = mapBlockIndex[proposal.blockhash]->GetBlockTime();
    }

    uint64_t deadline = proptime + proposal.nDeadline - pindexBestHeader->GetBlockTime();


    //use guiutil formatDurationStr function here
    //QString owo = GUIUtil::formatDurationStr(int(deadline));
    //std::cout << owo.toStdString() << "\n";

    uint64_t deadline_d = std::floor(deadline/86400);
    uint64_t deadline_h = std::floor((deadline-deadline_d*86400)/3600);
    uint64_t deadline_m = std::floor((deadline-(deadline_d*86400 + deadline_h*3600))/60);

    std::string s_deadline = "";
    if(deadline_d >= 14)
    {
        s_deadline = std::to_string(deadline_d) + std::string(" Days");
    }
    else
    {
        s_deadline = std::to_string(deadline_d) + std::string(" Days ") + std::to_string(deadline_h) + std::string(" Hours ") + std::to_string(deadline_m) + std::string(" Minutes");
    }

    ui->labelDuration->setText(QString::fromStdString(s_deadline));

    // Hide ability to vote is the status is expired
    std::string status = ui->labelStatus->text().toStdString();
    if (status.find("expired") != string::npos) {
        ui->buttonBoxVote->setStandardButtons(QDialogButtonBox::NoButton);
    }

    // If proposal is pending show voting cycles left
    if (proposal.fState == CFund::NIL) {
        std::string duration_title = "Voting Cycle: ";
        std::string duration = std::to_string(proposal.nVotingCycle) +  " of 6";
        ui->labelTitleDuration->setText(QString::fromStdString(duration_title));
        ui->labelDuration->setText(QString::fromStdString(duration));
    }

    // If proposal is rejected, show when it was rejected
    if (proposal.fState == CFund::REJECTED) {
        std::string expiry_title = "Rejected on: ";
        std::time_t t = static_cast<time_t>(proptime);
        std::stringstream ss;
        ss << std::put_time(std::gmtime(&t), "%c %Z");
        ui->labelTitleDuration->setText(QString::fromStdString(expiry_title));
        ui->labelDuration->setText(QString::fromStdString(ss.str().erase(10, 9)));
    }

    // If expired show when it expired
    if (proposal.fState == CFund::EXPIRED || status.find("expired") != string::npos) {
        if (proposal.fState == CFund::EXPIRED) {
            std::string expiry_title = "Expired on: ";
            std::time_t t = static_cast<time_t>(proptime);
            std::stringstream ss;
            ss << std::put_time(std::gmtime(&t), "%c %Z");
            ui->labelTitleDuration->setText(QString::fromStdString(expiry_title));
            ui->labelDuration->setText(QString::fromStdString(ss.str().erase(10, 9)));
        }
        else {
            std::string expiry_title = "Expires: ";
            std::string expiry = "At end of voting period";
            ui->labelTitleDuration->setText(QString::fromStdString(expiry_title));
            ui->labelDuration->setText(QString::fromStdString(expiry));
        }
    }

    // Shade in yes/no buttons is user has voted
    // If the proposal is pending and not prematurely expired (ie can be voted on):
    if (proposal.fState == CFund::NIL && proposal.GetState(pindexBestHeader->GetBlockTime()).find("expired") == string::npos) {
        // Get proposal votes list
        CFund::CProposal prop = this->proposal;
        auto it = std::find_if( vAddedProposalVotes.begin(), vAddedProposalVotes.end(),
                                [&prop](const std::pair<std::string, bool>& element){ return element.first == prop.hash.ToString();} );
        if (it != vAddedProposalVotes.end()) {
            if (it->second) {
                // Proposal was voted yes, shade in yes button and unshade no button
                ui->buttonBoxVote->button(QDialogButtonBox::Yes)->setStyleSheet(COLOR_VOTE_YES);
                ui->buttonBoxVote->button(QDialogButtonBox::No)->setStyleSheet(COLOR_VOTE_NEUTRAL);
            }
            else {
                // Proposal was noted no, shade in no button and unshade yes button
                ui->buttonBoxVote->button(QDialogButtonBox::Yes)->setStyleSheet(COLOR_VOTE_NEUTRAL);
                ui->buttonBoxVote->button(QDialogButtonBox::No)->setStyleSheet(COLOR_VOTE_NO);
            }
        }
        else {
            // Proposal was not voted on, reset shades of both buttons
            ui->buttonBoxVote->button(QDialogButtonBox::Yes)->setStyleSheet(COLOR_VOTE_NEUTRAL);
            ui->buttonBoxVote->button(QDialogButtonBox::No)->setStyleSheet(COLOR_VOTE_NEUTRAL);

        }
    }

    //hide ui voting elements on proposals which are not allowed vote states
    if(!proposal.CanVote())
    {
        ui->buttonBoxVote->setStandardButtons(QDialogButtonBox::NoButton);
    }

    std::string title_string = proposal.strDZeel;
    if (title_string.length() > 140) {
        title_string = title_string.substr(0, 140);
        title_string.append("...");
    }
    ui->title->setText(QString::fromStdString(title_string));

    //hide cancel button on proposals which have not been voted on

    std::string cur_hash = proposal.hash.ToString();
    for(std::pair<std::string, bool> obj: vAddedProposalVotes)
    {
        //add cancel button on hash match
        if(obj.first == cur_hash)
        {
            ui->buttonBoxVote->setStandardButtons(QDialogButtonBox::No|QDialogButtonBox::Yes|QDialogButtonBox::Cancel);

            // add colour to vote type
            if(obj.second == true)
            {
                ui->buttonBoxVote->button(QDialogButtonBox::Yes)->setStyleSheet(COLOR_VOTE_YES);
                ui->buttonBoxVote->button(QDialogButtonBox::No)->setStyleSheet(COLOR_VOTE_NEUTRAL);
            }
            else
            {
                ui->buttonBoxVote->button(QDialogButtonBox::Yes)->setStyleSheet(COLOR_VOTE_NEUTRAL);
                ui->buttonBoxVote->button(QDialogButtonBox::No)->setStyleSheet(COLOR_VOTE_NO);
            }
        }
    }

}

void CommunityFundDisplay::click_buttonBoxVote(QAbstractButton *button)
{
    //cast the vote
    bool duplicate = false;

    if (ui->buttonBoxVote->buttonRole(button) == QDialogButtonBox::YesRole)
    {
        ui->buttonBoxVote->setStandardButtons(QDialogButtonBox::No|QDialogButtonBox::Yes|QDialogButtonBox::Cancel);
        ui->buttonBoxVote->button(QDialogButtonBox::Yes)->setStyleSheet(COLOR_VOTE_YES);
        ui->buttonBoxVote->button(QDialogButtonBox::No)->setStyleSheet(COLOR_VOTE_NEUTRAL);
        CFund::VoteProposal(proposal.hash.ToString(), true, duplicate);
    }
    else if(ui->buttonBoxVote->buttonRole(button) == QDialogButtonBox::NoRole)
    {
        ui->buttonBoxVote->setStandardButtons(QDialogButtonBox::No|QDialogButtonBox::Yes|QDialogButtonBox::Cancel);
        ui->buttonBoxVote->button(QDialogButtonBox::Yes)->setStyleSheet(COLOR_VOTE_NEUTRAL);
        ui->buttonBoxVote->button(QDialogButtonBox::No)->setStyleSheet(COLOR_VOTE_NO);
        CFund::VoteProposal(proposal.hash.ToString(), false, duplicate);
    }
    else if(ui->buttonBoxVote->buttonRole(button) == QDialogButtonBox::RejectRole)
    {
        ui->buttonBoxVote->setStandardButtons(QDialogButtonBox::No|QDialogButtonBox::Yes);
        ui->buttonBoxVote->button(QDialogButtonBox::Yes)->setStyleSheet(COLOR_VOTE_NEUTRAL);
        ui->buttonBoxVote->button(QDialogButtonBox::No)->setStyleSheet(COLOR_VOTE_NEUTRAL);
        CFund::RemoveVoteProposal(proposal.hash.ToString());
    }
    else {
        return;
    }
}

void CommunityFundDisplay::click_pushButtonDetails()
{
    CommunityFundDisplayDetailed dlg(this, proposal);
    dlg.exec();
    refresh();

}

CommunityFundDisplay::~CommunityFundDisplay()
{
    delete ui;
}