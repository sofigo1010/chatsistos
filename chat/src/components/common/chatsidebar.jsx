"use client";
import { useState, useEffect, useRef } from "react";
import { MoreVertical } from "lucide-react"; 

export default function ChatSidebar() {
    const [searchTerm, setSearchTerm] = useState("");
    const [hovered, setHovered] = useState(null);
    const [activePos, setActivePos] = useState(null);
    const [activeButton, setActiveButton] = useState("Privado");
    const navRef = useRef(null);

    const menuItems = ["Privado", "Grupo", "Usuarios"];
    
    useEffect(() => {
        if (navRef.current) {
            const activeButtonElement = navRef.current.querySelector(`[data-name="${activeButton}"]`);
            if (activeButtonElement) {
                setActivePos({
                    left: activeButtonElement.offsetLeft,
                    width: activeButtonElement.offsetWidth,
                });
            }
        }
    }, [activeButton]);

    const handleHover = (e) => {
        if (e.target.dataset.name) {
            setHovered({
                left: e.target.offsetLeft,
                width: e.target.offsetWidth,
            });
        }
    };

    const handleMouseLeave = () => {
        setHovered(null);
    };

    const handleClick = (item) => {
        setActiveButton(item);
    };

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
                <button className="text-gray-400 hover:text-white text-2xl">
                    <MoreVertical size={20} />
                </button>
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
            <nav
                ref={navRef}
                className="relative flex justify-center gap-6 px-4 py-2 bg-[#0a0f1a] rounded-full mb-4 overflow-hidden"
                onMouseLeave={handleMouseLeave}
            >
                <div
                    className="absolute top-0 bottom-0 bg-[var(--color-baby-blue)] rounded-full transition-all duration-500 ease-out"
                    style={{
                        left: (hovered || activePos)?.left || 0,
                        width: (hovered || activePos)?.width || 0,
                    }}
                ></div>

                {menuItems.map((item) => (
                    <button
                        key={item}
                        data-name={item}
                        onClick={() => handleClick(item)}
                        onMouseEnter={handleHover}
                        className={`relative px-4 py-1 rounded-lg text-sm transition-all duration-300
                            ${activeButton === item ? "text-black z-10" : "text-white"} 
                            hover:text-black`}
                    >
                        {item}
                    </button>
                ))}
            </nav>

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
