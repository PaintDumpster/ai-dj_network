export type FlowState = 
| "Welcome"
| "Info"
| "Game"
| "Results"
| "Log";

export type FlowEvent =
| "PREVIOUS"
| "CONTINUE"
| "NEXT";

export const transition = (state: FlowState, event: FlowEvent): FlowState => {
    switch (state) {
        case "Welcome":
            if (event === "NEXT") return "Info";
            return "Welcome";
        case "Info":
            if (event === "PREVIOUS") return "Welcome";
            if (event === "NEXT") return "Game";
            return "Info";
        case "Game":
            if (event === "PREVIOUS") return "Info";
            if (event === "NEXT") return "Results";
            return "Game";
        case "Results":
            if (event === "PREVIOUS") return "Game";
            if (event === "NEXT") return "Log";
            return "Results";
        case "Log":
            if (event === "PREVIOUS") return "Results";
            return "Log";

        default:
            return state;
    }
};
