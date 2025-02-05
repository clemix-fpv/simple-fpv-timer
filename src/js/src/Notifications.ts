
import van from "./lib/van-1.5.2.js"
const {h5, div} = van.tags
import { Modal } from './lib/bootstrap.bundle.js'

export enum NotificationStatus {
    MODAL,
    ERROR,
    SUCCESS,
    WARNING,
}

export interface NotificationEvent {
    msg: string;
    title?: string;
    status?: NotificationStatus;
}

export class Notifications {

    public static showSuccess(n: NotificationEvent) {
        n.status = NotificationStatus.SUCCESS;
        Notifications.show(n);
    }

    public static showError(n: NotificationEvent) {
        n.status = NotificationStatus.ERROR;
        Notifications.show(n);
    }

    public static showWarning(n: NotificationEvent) {
        n.status = NotificationStatus.WARNING;
        Notifications.show(n);
    }

    public static show(n:NotificationEvent) {
        document.dispatchEvent(
            new CustomEvent("SFT_NOTIFICATION", {detail: n})
        );
    }

    public createModal(n: NotificationEvent) {
        var content_class = "modal-content";
        if (!n.status)
            n.status = NotificationStatus.MODAL;

        switch(n.status) {
            case NotificationStatus.SUCCESS:
                content_class = "alert alert-success";
                break;
            case NotificationStatus.ERROR:
                content_class = "alert alert-danger";
        }
        var div_content = div({class: content_class});

        if (n.title)
            van.add(div_content,
                div({class: "modal-header"},
                    h5({class: "modal-title"}, n.title),
                )
            );

        if (n.msg)
            van.add(div_content,
                div({class: "modal-body"}, n.msg)
            );
        var d = div({class:"modal fade ", tabindex: "-1", role: "dialog"},
            div({class: "modal-dialog", role: "document"},
                div_content
            )
        );
        van.add(document.body, d);
        return new Modal(d);
    }

    public showNotification(n: NotificationEvent) {
        var modal = this.createModal(n);
        modal.show();
        setTimeout(() => {
            modal.hide();
            setTimeout(() => {
                modal.dispose();
                }, 5000);
        }, 1500);
    }

    constructor() {
        document.addEventListener("SFT_NOTIFICATION",
            (e: CustomEventInit<NotificationEvent>) => {
            if (e.detail)
                this.showNotification(e.detail);
        })
    }
}
