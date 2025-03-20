"use client";
import { useState } from "react";
import Image from "next/image";

export default function ChatSidebar() {
    const [searchTerm, setSearchTerm] = useState("");
    const chats = [
        { name: "Bevstack Support", message: "You: hi @~Anirudh Reddy...", time: "4:58 pm" },
        { name: "Gordish <3", message: "y tlj q no se pq no me sale...", time: "9:30 pm" },
        { name: "Ignora", message: "You: https://productmanagement...", time: "6:27 pm" },
        { name: "Julio Hayserr 😃", message: "JAJAJ por q", time: "9:32 pm" },
        { name: "Sofi (You)", message: "Draft: npx create-next-app@latest...", time: "27/02/2025" },
        { name: "+502 4693 1033", message: "Draft: npx create-next-app@latest...", time: "9:27 pm" },
    ];

    return (
        <div className="bg-gradient-to-b from-[#030712] to-[#0a0f1a] text-white h-screen w-80 flex flex-col">
            <div className="flex items-center justify-between p-4 border-b border-gray-700">
                <h1 className="text-xl font-bold">Chats</h1>
                <button className="text-gray-400 hover:text-white text-2xl">+</button>
            </div>
            <div className="p-4">
                <input 
                    type="text"
                    value={searchTerm}
                    onChange={(e) => setSearchTerm(e.target.value)}
                    placeholder="Search"
                    className="w-full bg-gray-800 text-white p-2 rounded-lg focus:outline-none"
                />
            </div>
            <div className="flex space-x-2 px-4 mb-4">
                {['All', 'Unread', 'Favorites', 'Groups'].map(filter => (
                    <button
                        key={filter}
                        className="bg-gray-800 text-gray-400 text-sm px-3 py-1 rounded-full hover:bg-gray-700"
                    >
                        {filter}
                    </button>
                ))}
            </div>
            <div className="overflow-y-auto flex-grow space-y-1 px-4">
                {chats.filter(chat => chat.name.toLowerCase().includes(searchTerm.toLowerCase())).map((chat, index) => (
                    <div 
                        key={index}
                        className="flex items-center justify-between p-3 rounded-lg hover:bg-gray-800 cursor-pointer"
                    >
                        <div>
                            <h2 className="text-sm font-medium">{chat.name}</h2>
                            <p className="text-xs text-gray-400 truncate">{chat.message}</p>
                        </div>
                        <span className="text-xs text-gray-400">{chat.time}</span>
                    </div>
                ))}
            </div>
        </div>
    );
}
