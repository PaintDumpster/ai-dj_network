import { useState } from "react";
import type { FlowState, FlowEvent } from "../state/flowMachine";
import { transition } from "../state/flowMachine";

export function useFlowMachine(initial: FlowState = "Welcome") {
    const [currentPage, setCurrentpage] = useState<FlowState>(initial);

    const send = (event: FlowEvent) => {
        setCurrentpage(prev => transition(prev, event));
    };

    return { currentPage, send};
}