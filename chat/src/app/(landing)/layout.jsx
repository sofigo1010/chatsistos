import ChatSidebar from "@/components/common/chatsidebar";
import "../../styles/globals.css"; 

export default function ChatLayout({ children }) {
    return (
        <html lang="es">
            <body className="bg-gradient-to-b from-[#030712] to-[#0a0f1a] text-white flex flex-col min-h-screen">
                <div className="flex flex-grow">
                    <ChatSidebar />
                    <main className="flex-grow p-10">{children}</main>
                </div>
            </body>
        </html>
    );
}
