#pragma once
#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>

namespace prototype::graphical {

    class LoginDialog : public QDialog {
        Q_OBJECT

    public:
        explicit LoginDialog(QWidget *parent = nullptr);

        QString getUsername() const { return user_edit->text(); }
        QString getPassword() const { return pass_edit->text(); }
        bool isRegistering() const { return mode_register; }

    private slots:
        void onLogin();
        void onRegister();

    private:
        QLineEdit *user_edit;
        QLineEdit *pass_edit;
        bool mode_register = false;

        void setup_ui();
    };

}
