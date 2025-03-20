"use client";
import { useEffect, useState } from "react";
import { useRouter } from "next/navigation";
import { Send } from "lucide-react"; 

export default function ChatPage() {
    const router = useRouter();
    const [userName, setUserName] = useState("");
    const [messages, setMessages] = useState([]);
    const [input, setInput] = useState("");

    useEffect(() => {
        const name = localStorage.getItem("name");
        if (!name) {
            router.push('/');
        } else {
            setUserName(name);
        }
    }, [router]);

    const handleSendMessage = () => {
        if (input.trim() !== "") {
            const newMessage = { user: userName, text: input };
            setMessages([...messages, newMessage]);
            setInput("");
        }
    };

    return (
        <div className="flex flex-col w-full h-full bg-[#0a0f1a] rounded-xl shadow-xl">
            <div className="flex items-center justify-between p-4 border-b border-gray-700">
                <h2 className="text-xl font-bold text-white">Chat with Bevstack Support</h2>
            </div>

            <div className="flex-grow overflow-y-auto p-4 space-y-4">
                {messages.map((message, index) => (
                    <div 
                        key={index} 
                        className={`flex ${message.user === userName ? 'justify-end' : 'justify-start'}`}
                    >
                        <div className={`max-w-xs p-3 rounded-lg ${message.user === userName ? 'bg-[#1d4ed8]' : 'bg-[#16171d]'}`}>
                            <p className="text-white">{message.text}</p>
                        </div>
                    </div>
                ))}
            </div>
            <div className="flex items-center p-4 border-t border-gray-700 bg-[#16171d]">
                <input
                    type="text"
                    value={input}
                    onChange={(e) => setInput(e.target.value)}
                    placeholder="Write a message..."
                    className="flex-grow p-2 rounded-lg bg-[#0a0f1a] text-white focus:outline-none"
                />
                <button 
                    onClick={handleSendMessage}
                    className="ml-4 p-2 bg-[var(--color-baby-blue)] rounded-full hover:bg-[#71a6d2] transition-all"
                >
                    <Send size={20} color="black" />
                </button>
            </div>
        </div>
    );
}
