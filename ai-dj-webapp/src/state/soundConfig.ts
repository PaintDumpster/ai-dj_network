export const soundFolders = {
    b1:"/sounds/1. Ciutat Vella",
    b2:"/sounds/2. Eixample",
    b3:"/sounds/3. Sants-Montjuic",
    b4:"/sounds/4. Les Corts",
    b5:"/sounds/5. Sarrià-St Gervasi",
    b6:"/sounds/6. Gràcia",
    b7:"/sounds/7. Horta-Guinardó",
    b8:"/sounds/8. Nou Barris",
    b9:"/sounds/9. Sant Andreu",
    b10:"/sounds/10. Sant Martí",
} as const;

export type SoundButton = keyof typeof soundFolders;